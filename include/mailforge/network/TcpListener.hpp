#pragma once

#include "mailforge/config/ServerConfig.hpp"
#include "mailforge/logging/Logger.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace mailforge::network {

class TcpListener {
public:
    TcpListener(boost::asio::io_context& io_context,
                const config::ServerConfig& config,
                logging::Logger& logger);

    void start();

private:
    void accept_next();

    boost::asio::ip::tcp::acceptor acceptor_;
    config::ServerConfig config_;
    logging::Logger& logger_;
};

} // namespace mailforge::network
