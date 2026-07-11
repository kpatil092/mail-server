#include "mailforge/smtp/SmtpClient.hpp"
#include <boost/asio/write.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/connect.hpp>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <memory>
#include <regex>

namespace mailforge::smtp {

// Private helper class to run the client-side SMTP exchange asynchronously
class OutgoingSession : public std::enable_shared_from_this<OutgoingSession> {
public:
    OutgoingSession(boost::asio::io_context& io_context,
                    logging::Logger& logger,
                    std::string host,
                    std::string sender,
                    std::string recipient,
                    std::string mail_data,
                    std::function<void(bool success)> callback)
        : resolver_(io_context),
          socket_(io_context),
          logger_(logger),
          host_(std::move(host)),
          sender_(std::move(sender)),
          recipient_(std::move(recipient)),
          mail_data_(std::move(mail_data)),
          callback_(std::move(callback)) {}

    void start() {
        auto self = shared_from_this();
        resolver_.async_resolve(host_, "25",
            [this, self](const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results) {
                if (ec) {
                    logger_.error("Failed to resolve " + host_ + ": " + ec.message());
                    callback_(false);
                    return;
                }
                connect(results);
            });
    }

private:
    void connect(const boost::asio::ip::tcp::resolver::results_type& results) {
        auto self = shared_from_this();
        boost::asio::async_connect(socket_, results,
            [this, self](const boost::system::error_code& ec, const boost::asio::ip::tcp::endpoint&) {
                if (ec) {
                    logger_.error("Failed to connect to " + host_ + ": " + ec.message());
                    callback_(false);
                    return;
                }
                read_response([this, self](const std::string& line) {
                    if (line.rfind("220", 0) != 0) {
                        logger_.error("SMTP greeting error from " + host_ + ": " + line);
                        callback_(false);
                        return;
                    }
                    send_ehlo();
                });
            });
    }

    void send_ehlo() {
        auto self = shared_from_this();
        send_command("EHLO localhost\r\n", [this, self](const std::string& line) {
            if (line.rfind("250", 0) != 0) {
                logger_.error("EHLO rejected by " + host_ + ": " + line);
                callback_(false);
                return;
            }
            send_mail_from();
        });
    }

    void send_mail_from() {
        auto self = shared_from_this();
        send_command("MAIL FROM:<" + sender_ + ">\r\n", [this, self](const std::string& line) {
            if (line.rfind("250", 0) != 0) {
                logger_.error("MAIL FROM rejected by " + host_ + ": " + line);
                callback_(false);
                return;
            }
            send_rcpt_to();
        });
    }

    void send_rcpt_to() {
        auto self = shared_from_this();
        send_command("RCPT TO:<" + recipient_ + ">\r\n", [this, self](const std::string& line) {
            if (line.rfind("250", 0) != 0) {
                logger_.error("RCPT TO rejected by " + host_ + ": " + line);
                callback_(false);
                return;
            }
            send_data();
        });
    }

    void send_data() {
        auto self = shared_from_this();
        send_command("DATA\r\n", [this, self](const std::string& line) {
            if (line.rfind("354", 0) != 0) {
                logger_.error("DATA command rejected by " + host_ + ": " + line);
                callback_(false);
                return;
            }
            send_body();
        });
    }

    void send_body() {
        auto self = shared_from_this();
        // Append period on its own line to end data
        auto response = std::make_shared<std::string>(mail_data_ + "\r\n.\r\n");
        boost::asio::async_write(socket_, boost::asio::buffer(*response),
            [this, self, response](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    logger_.error("Failed to write mail body: " + ec.message());
                    callback_(false);
                    return;
                }
                read_response([this, self](const std::string& line) {
                    if (line.rfind("250", 0) != 0) {
                        logger_.error("Data transmission failed on " + host_ + ": " + line);
                        callback_(false);
                        return;
                    }
                    send_quit();
                });
            });
    }

    void send_quit() {
        auto self = shared_from_this();
        auto cmd = std::make_shared<std::string>("QUIT\r\n");
        boost::asio::async_write(socket_, boost::asio::buffer(*cmd),
            [this, self, cmd](const boost::system::error_code&, std::size_t) {
                logger_.info("Successfully delivered mail to " + host_);
                callback_(true);
                close();
            });
    }

    template <typename Handler>
    void send_command(const std::string& cmd, Handler&& handler) {
        auto self = shared_from_this();
        auto cmd_ptr = std::make_shared<std::string>(cmd);
        boost::asio::async_write(socket_, boost::asio::buffer(*cmd_ptr),
            [this, self, cmd_ptr, h = std::forward<Handler>(handler)](const boost::system::error_code& ec, std::size_t) mutable {
                if (ec) {
                    logger_.error("Write failed: " + ec.message());
                    callback_(false);
                    return;
                }
                read_response(std::move(h));
            });
    }

    template <typename Handler>
    void read_response(Handler&& handler) {
        auto self = shared_from_this();
        boost::asio::async_read_until(socket_, buffer_, "\n",
            [this, self, h = std::forward<Handler>(handler)](const boost::system::error_code& ec, std::size_t) mutable {
                if (ec) {
                    logger_.error("Read failed: " + ec.message());
                    callback_(false);
                    return;
                }
                std::istream is(&buffer_);
                std::string line;
                std::getline(is, line);
                h(line);
            });
    }

    void close() {
        boost::system::error_code ignored;
        socket_.close(ignored);
    }

    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf buffer_;
    logging::Logger& logger_;
    std::string host_;
    std::string sender_;
    std::string recipient_;
    std::string mail_data_;
    std::function<void(bool success)> callback_;
};

// SmtpClient Implementation
SmtpClient::SmtpClient(boost::asio::io_context& io_context, logging::Logger& logger)
    : io_context_(io_context), logger_(logger) {}

void SmtpClient::deliver(const std::string& sender,
                         const std::string& recipient,
                         const std::string& subject,
                         const std::string& body,
                         std::function<void(bool success)> callback) {
    std::string domain = recipient.substr(recipient.find('@') + 1);
    // Strip trailing/leading brackets if present
    if (!domain.empty() && domain.front() == '<') domain.erase(0, 1);
    if (!domain.empty() && domain.back() == '>') domain.pop_back();

    std::vector<std::string> hosts = resolve_mx(domain);
    if (hosts.empty()) {
        hosts.push_back(domain); // Fall back to direct domain name (A/AAAA record)
    }

    std::ostringstream mail_content;
    mail_content << "From: " << sender << "\r\n"
                 << "To: " << recipient << "\r\n"
                 << "Subject: " << subject << "\r\n"
                 << "\r\n"
                 << body;

    try_deliver_to_hosts(hosts, 0, sender, recipient, mail_content.str(), std::move(callback));
}

std::vector<std::string> SmtpClient::resolve_mx(const std::string& domain) {
    std::vector<std::string> hosts;
    std::string command = "host -t mx " + domain + " 2>/dev/null";
    
    struct PcloseDeleter {
        void operator()(FILE* f) const {
            if (f) pclose(f);
        }
    };
    
    std::unique_ptr<FILE, PcloseDeleter> pipe(popen(command.c_str(), "r"));
    if (!pipe) {
        return hosts;
    }

    char buffer[256];
    std::vector<std::pair<int, std::string>> records;
    std::regex mx_regex("mail is handled by (\\d+) (\\S+)\\.");
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        std::string line(buffer);
        std::smatch match;
        if (std::regex_search(line, match, mx_regex) && match.size() == 3) {
            int priority = std::stoi(match[1].str());
            std::string host = match[2].str();
            records.emplace_back(priority, host);
        }
    }

    // Sort by priority (lowest preference value is highest priority)
    std::sort(records.begin(), records.end());
    for (const auto& rec : records) {
        hosts.push_back(rec.second);
    }
    return hosts;
}

void SmtpClient::try_deliver_to_hosts(const std::vector<std::string>& hosts,
                                      std::size_t host_index,
                                      const std::string& sender,
                                      const std::string& recipient,
                                      const std::string& mail_data,
                                      std::function<void(bool success)> callback) {
    if (host_index >= hosts.size()) {
        logger_.error("All delivery attempts failed for recipient: " + recipient);
        callback(false);
        return;
    }

    std::string current_host = hosts[host_index];
    logger_.info("Attempting SMTP delivery to " + current_host + " (attempt " + std::to_string(host_index + 1) + ")");

    auto session = std::make_shared<OutgoingSession>(
        io_context_, logger_, current_host, sender, recipient, mail_data,
        [this, hosts, host_index, sender, recipient, mail_data, callback](bool success) {
            if (success) {
                callback(true);
            } else {
                // Try next host recursively
                try_deliver_to_hosts(hosts, host_index + 1, sender, recipient, mail_data, callback);
            }
        });
    session->start();
}

} // namespace mailforge::smtp
