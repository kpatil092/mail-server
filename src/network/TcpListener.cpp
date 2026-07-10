#include "mailforge/network/TcpListener.hpp"

#include "mailforge/network/ClientSession.hpp"

#include <boost/asio/ip/address.hpp>

#include <sstream>

namespace mailforge::network {

TcpListener::TcpListener(boost::asio::io_context& io_context,
                         const config::ServerConfig& config,
                         logging::Logger& logger)
    : acceptor_(io_context),
      config_(config),
      logger_(logger) {
    const auto address = boost::asio::ip::make_address(config_.host);
    const boost::asio::ip::tcp::endpoint endpoint(address, config_.port);

    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(static_cast<int>(config_.max_connections));
}

void TcpListener::start() {
    std::ostringstream message;
    message << "SMTP listener accepting connections on " << config_.host << ':' << config_.port;
    logger_.info(message.str());
    accept_next();
}

void TcpListener::accept_next() {
    acceptor_.async_accept([this](const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
        if (error) {
            logger_.error("Accept failed: " + error.message());
        } else {
            logger_.info("Accepted SMTP client from " + socket.remote_endpoint().address().to_string());
            auto session = std::make_shared<ClientSession>(std::move(socket), config_.server_name,
                                                          config_.mail_directory, logger_);
            session->start();
        }

        accept_next();
    });
}

} // namespace mailforge::network
