#include "mailforge/network/ClientSession.hpp"
#include "mailforge/mime/MailMessage.hpp"
#include "mailforge/database/DatabaseManager.hpp"
#include "mailforge/queue/QueueManager.hpp"

#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include <istream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <sstream>
#include <vector>

namespace mailforge::network {
namespace {

void trim_crlf_inplace(std::string &line) {
  const auto pos = line.find_last_not_of("\r\n");
  if (pos != std::string::npos) {
    line.erase(pos + 1);
  }
}

std::string wire_reply(const smtp::SMTPReply &reply) {
  std::string result;
  std::string line;
  std::istringstream stream(reply.message);
  std::vector<std::string> lines;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(line);
  }
  
  if (lines.empty()) {
    return std::to_string(reply.code) + " \r\n";
  }
  
  for (std::size_t i = 0; i < lines.size(); ++i) {
    bool is_last = (i == lines.size() - 1);
    result += std::to_string(reply.code) + (is_last ? " " : "-") + lines[i] + "\r\n";
  }
  return result;
}

bool is_local_recipient(const std::string &recipient,
                        const std::string &server_name) {
  auto email = recipient;
  if (email.size() >= 2 && email.front() == '<' && email.back() == '>') {
    email = email.substr(1, email.size() - 2);
  }
  const auto at_pos = email.find('@');
  if (at_pos == std::string::npos) {
    return true;
  }
  return email.substr(at_pos + 1) == server_name;
}

std::string extract_subject(const std::string& raw_content) {
  std::string line;
  std::istringstream stream(raw_content);
  while (std::getline(stream, line)) {
    if (line.empty() || line == "\r") {
      break;
    }
    if (line.rfind("Subject: ", 0) == 0 || line.rfind("subject: ", 0) == 0) {
      std::string sub = line.substr(9);
      if (!sub.empty() && sub.back() == '\r') sub.pop_back();
      return sub;
    }
  }
  return "";
}

std::string extract_body(const std::string& raw_content) {
  std::size_t pos = raw_content.find("\n\n");
  if (pos != std::string::npos) {
    return raw_content.substr(pos + 2);
  }
  pos = raw_content.find("\r\n\r\n");
  if (pos != std::string::npos) {
    return raw_content.substr(pos + 4);
  }
  return raw_content;
}

} // namespace

ClientSession::ClientSession(boost::asio::ip::tcp::socket socket,
                             std::string server_name,
                             std::filesystem::path mail_directory,
                             logging::Logger &logger,
                             database::DatabaseManager* db,
                             queue::QueueManager* queue,
                             boost::asio::ssl::context* ssl_context)
    : socket_(std::move(socket)), server_name_(server_name),
      storage_(std::move(mail_directory)),
      state_machine_(std::move(server_name), db), logger_(logger), db_(db), queue_(queue),
      ssl_context_(ssl_context) {}

void ClientSession::start() { write_reply(state_machine_.greeting()); }

void ClientSession::read_line() {
  auto self = shared_from_this();
  auto handler = [this, self](const boost::system::error_code &error, std::size_t) {
    if (error) {
      close();
      return;
    }

    std::istream input_stream(&input_);
    std::string line;
    std::getline(input_stream, line);
    trim_crlf_inplace(line);

    if (state_machine_.is_receiving_data()) {
      if (line == ".") {
        const auto sender = state_machine_.sender();
        const auto recipients = state_machine_.recipients();

        bool all_local = true;
        for (const auto &recipient : recipients) {
          if (!is_local_recipient(recipient, server_name_)) {
            all_local = false;
            break;
          }
        }

        if (!all_local && !state_machine_.is_authenticated()) {
          logger_.error(
              "Rejected message delivery: non-local recipient found and session is unauthenticated");
          data_buffer_.clear();
          (void)state_machine_.handle(parser_.parse_line("RSET"));
          write_reply(smtp::SMTPReply{554, "Transaction failed"});
          return;
        }

        try {
          std::vector<std::string> local_recipients;
          for (const auto& r : recipients) {
            if (is_local_recipient(r, server_name_)) {
              local_recipients.push_back(r);
            }
          }
          if (!local_recipients.empty()) {
            storage_.store(
                mime::MailMessage(sender, local_recipients, data_buffer_));
          }
          
          if (db_) {
            std::string subject = extract_subject(data_buffer_);
            std::string body = extract_body(data_buffer_);
            for (const auto& recipient : recipients) {
              std::string status = is_local_recipient(recipient, server_name_) ? "delivered" : "pending";
              db_->store_message(sender, recipient, subject, body, status);
            }
          }
          
          if (queue_) {
            queue_->notify();
          }
          
          logger_.info("Stored email for " +
                       std::to_string(recipients.size()) + " recipient(s)");
        } catch (const std::exception &error) {
          logger_.error("Mail storage failed: " +
                        std::string(error.what()));
          data_buffer_.clear();
          (void)state_machine_.handle(parser_.parse_line("RSET"));
          write_reply(smtp::SMTPReply{554, "Transaction failed"});
          return;
        }
        data_buffer_.clear();
        write_reply(state_machine_.complete_data());
        return;
      }
      data_buffer_.append(line + "\n");
      read_line(); // Continue reading the next line of the email body
      return;
    }

    write_reply(state_machine_.handle(parser_.parse_line(line)));
  };

  if (ssl_stream_) {
    boost::asio::async_read_until(*ssl_stream_, input_, "\n", handler);
  } else {
    boost::asio::async_read_until(socket_, input_, "\n", handler);
  }
}

void ClientSession::write_reply(const smtp::SMTPReply &reply) {
  auto self = shared_from_this();
  const auto close_connection = reply.close_connection;
  const bool start_tls = (reply.code == 220 && reply.message == "Ready to start TLS");
  auto response = std::make_shared<std::string>(wire_reply(reply));

  auto write_handler = [this, self, response, close_connection, start_tls](const boost::system::error_code &error, std::size_t) {
    if (error || close_connection) {
      close();
      return;
    }
    if (start_tls) {
      if (!ssl_context_) {
        logger_.error("SSL context is not configured, closing session");
        close();
        return;
      }
      ssl_stream_ = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(std::move(socket_), *ssl_context_);
      ssl_stream_->async_handshake(boost::asio::ssl::stream_base::server,
          [this, self](const boost::system::error_code& handshake_error) {
            if (handshake_error) {
              logger_.error("SSL handshake failed: " + handshake_error.message());
              close();
              return;
            }
            logger_.info("SSL handshake successful");
            read_line();
          });
      return;
    }
    read_line();
  };

  if (ssl_stream_) {
    boost::asio::async_write(*ssl_stream_, boost::asio::buffer(*response), write_handler);
  } else {
    boost::asio::async_write(socket_, boost::asio::buffer(*response), write_handler);
  }
}

void ClientSession::close() {
  boost::system::error_code ignored;
  if (ssl_stream_) {
    ssl_stream_->shutdown(ignored);
    ssl_stream_->lowest_layer().close(ignored);
  } else {
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored);
    socket_.close(ignored);
  }
}

} // namespace mailforge::network
