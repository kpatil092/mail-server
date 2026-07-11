#pragma once
// Dummy comment to trigger clangd reparse of header

#include "mailforge/config/ServerConfig.hpp"
#include "mailforge/logging/Logger.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>

namespace mailforge::database {
class DatabaseManager;
}

namespace mailforge::queue {
class QueueManager;
}

namespace mailforge::network {

class TcpListener {
public:
    TcpListener(boost::asio::io_context& io_context,
                const config::ServerConfig& config,
                logging::Logger& logger,
                database::DatabaseManager* db = nullptr,
                queue::QueueManager* queue = nullptr);

    void start();

private:
    void accept_next();

    boost::asio::ip::tcp::acceptor acceptor_;
    config::ServerConfig config_;
    logging::Logger& logger_;
    database::DatabaseManager* db_{nullptr};
    queue::QueueManager* queue_{nullptr};
    std::unique_ptr<boost::asio::ssl::context> ssl_context_;
};

} // namespace mailforge::network
