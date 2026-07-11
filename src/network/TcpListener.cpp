#include "mailforge/network/TcpListener.hpp"

#include "mailforge/network/ClientSession.hpp"

#include <boost/asio/ip/address.hpp>

#include <sstream>

namespace mailforge::network {

TcpListener::TcpListener(boost::asio::io_context& io_context,
                         const config::ServerConfig& config,
                         logging::Logger& logger,
                         database::DatabaseManager* db,
                         queue::QueueManager* queue)
    : acceptor_(io_context),
      config_(config),
      logger_(logger),
      db_(db),
      queue_(queue) {
    const auto address = boost::asio::ip::make_address(config_.host);
    const boost::asio::ip::tcp::endpoint endpoint(address, config_.port);

    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(static_cast<int>(config_.max_connections));

    if (!config_.tls_certificate.empty() && !config_.tls_private_key.empty()) {
        try {
            ssl_context_ = std::make_unique<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
            ssl_context_->set_options(boost::asio::ssl::context::default_workarounds |
                                      boost::asio::ssl::context::no_sslv2 |
                                      boost::asio::ssl::context::no_sslv3 |
                                      boost::asio::ssl::context::single_dh_use);
            ssl_context_->use_certificate_chain_file(config_.tls_certificate);
            ssl_context_->use_private_key_file(config_.tls_private_key, boost::asio::ssl::context::pem);
            logger_.info("TLS certificate and private key loaded successfully");
        } catch (const std::exception& e) {
            logger_.error("Failed to load TLS configuration: " + std::string(e.what()));
            ssl_context_.reset();
        }
    }
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
                                                          config_.mail_directory, logger_, db_, queue_, ssl_context_.get());
            session->start();
        }

        accept_next();
    });
}

} // namespace mailforge::network
