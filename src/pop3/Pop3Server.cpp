#include "mailforge/pop3/Pop3Server.hpp"
#include <boost/asio/write.hpp>
#include <boost/asio/read_until.hpp>
#include <sstream>
#include <iostream>

namespace mailforge::pop3 {

// Pop3Session Implementation
Pop3Session::Pop3Session(boost::asio::ip::tcp::socket socket,
                         logging::Logger& logger,
                         database::DatabaseManager& db)
    : socket_(std::move(socket)),
      logger_(logger),
      state_machine_(db) {}

void Pop3Session::start() {
    write_reply(state_machine_.greeting());
}

void Pop3Session::read_line() {
    auto self = shared_from_this();
    boost::asio::async_read_until(
        socket_, input_, "\n",
        [this, self](const boost::system::error_code& error, std::size_t) {
            if (error) {
                close();
                return;
            }

            std::istream input_stream(&input_);
            std::string line;
            std::getline(input_stream, line);

            // Strip trailing \r if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            write_reply(state_machine_.handle(line));
        });
}

void Pop3Session::write_reply(const Pop3Reply& reply) {
    auto self = shared_from_this();
    auto response = std::make_shared<std::string>(reply.message + "\r\n");

    boost::asio::async_write(
        socket_, boost::asio::buffer(*response),
        [this, self, response, reply](const boost::system::error_code& error, std::size_t) {
            if (error || reply.close_connection) {
                close();
                return;
            }
            read_line();
        });
}

void Pop3Session::close() {
    boost::system::error_code ignored;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored);
    socket_.close(ignored);
}

// Pop3Server Implementation
Pop3Server::Pop3Server(boost::asio::io_context& io_context,
                       const config::ServerConfig& config,
                       logging::Logger& logger,
                       database::DatabaseManager& db)
    : acceptor_(io_context),
      config_(config),
      logger_(logger),
      db_(db) {
    const auto address = boost::asio::ip::make_address(config_.host);
    const boost::asio::ip::tcp::endpoint endpoint(address, config_.pop3_port);

    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(static_cast<int>(config_.max_connections));
}

void Pop3Server::start() {
    std::ostringstream message;
    message << "POP3 listener accepting connections on " << config_.host << ':' << config_.pop3_port;
    logger_.info(message.str());
    accept_next();
}

void Pop3Server::accept_next() {
    acceptor_.async_accept([this](const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
        if (error) {
            logger_.error("POP3 Accept failed: " + error.message());
        } else {
            logger_.info("Accepted POP3 client from " + socket.remote_endpoint().address().to_string());
            auto session = std::make_shared<Pop3Session>(std::move(socket), logger_, db_);
            session->start();
        }
        accept_next();
    });
}

} // namespace mailforge::pop3
