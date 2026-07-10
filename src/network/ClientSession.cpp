#include "mailforge/network/ClientSession.hpp"

#include "mailforge/mime/MailMessage.hpp"

#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include <istream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace mailforge::network {
namespace {

void trim_crlf_inplace(std::string& line) {
    const auto pos = line.find_last_not_of("\r\n");
    if (pos != std::string::npos) {
        line.erase(pos + 1);
    }
}

std::string wire_reply(const smtp::SMTPReply& reply) {
    return std::to_string(reply.code) + " " + reply.message + "\r\n";
}

} // namespace

ClientSession::ClientSession(boost::asio::ip::tcp::socket socket,
                             std::string server_name,
                             std::filesystem::path mail_directory,
                             logging::Logger& logger)
    : socket_(std::move(socket)),
      storage_(std::move(mail_directory)),
      state_machine_(std::move(server_name)),
      logger_(logger) {}

void ClientSession::start() {
    write_reply(state_machine_.greeting());
}

void ClientSession::read_line() {
    auto self = shared_from_this();
    boost::asio::async_read_until(socket_, input_, "\n", [this, self](const boost::system::error_code& error, std::size_t) {
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
                try {
                    storage_.store(mime::MailMessage(sender, recipients, data_buffer_));
                    logger_.info("Stored email for " + std::to_string(recipients.size()) + " recipient(s)");
                } catch (const std::exception& error) {
                    logger_.error("Mail storage failed: " + std::string(error.what()));
                    data_buffer_.clear();
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
    });
}

void ClientSession::write_reply(const smtp::SMTPReply& reply) {
    auto self = shared_from_this();
    const auto close_connection = reply.close_connection;
    auto response = std::make_shared<std::string>(wire_reply(reply));

    boost::asio::async_write(socket_, boost::asio::buffer(*response), [this, self, response, close_connection](const boost::system::error_code& error, std::size_t) {
        if (error || close_connection) {
            close();
            return;
        }
        read_line();
    });
}

void ClientSession::close() {
    // Gracefully shut down the sending side of the socket.
    // This sends a FIN packet to the client, signaling that we are done sending.
    boost::system::error_code ignored;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored);
    socket_.close(ignored);
}

} // namespace mailforge::network
