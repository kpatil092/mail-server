#pragma once

#include "mailforge/logging/Logger.hpp"
#include "mailforge/storage/MailStorage.hpp"
#include "mailforge/smtp/SMTPParser.hpp"
#include "mailforge/smtp/SMTPStateMachine.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/ssl.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace mailforge::database {
class DatabaseManager;
}

namespace mailforge::queue {
class QueueManager;
}

namespace boost::asio::ssl {
class context;
}

namespace mailforge::network {

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::ip::tcp::socket socket,
                  std::string server_name,
                  std::filesystem::path mail_directory,
                  logging::Logger& logger,
                  database::DatabaseManager* db = nullptr,
                  queue::QueueManager* queue = nullptr,
                  boost::asio::ssl::context* ssl_context = nullptr);

    void start();

private:
    void read_line();
    void write_reply(const smtp::SMTPReply& reply);
    void close();

    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf input_;
    smtp::SMTPParser parser_;
    std::string server_name_;
    std::string data_buffer_;
    storage::MailStorage storage_;
    smtp::SMTPStateMachine state_machine_;
    logging::Logger& logger_;
    database::DatabaseManager* db_{nullptr};
    queue::QueueManager* queue_{nullptr};
    boost::asio::ssl::context* ssl_context_{nullptr};
    std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> ssl_stream_;
};

} // namespace mailforge::network
