#pragma once

#include "mailforge/config/ServerConfig.hpp"
#include "mailforge/logging/Logger.hpp"
#include "mailforge/database/DatabaseManager.hpp"
#include "mailforge/pop3/Pop3StateMachine.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <memory>

namespace mailforge::pop3 {

class Pop3Session : public std::enable_shared_from_this<Pop3Session> {
public:
    Pop3Session(boost::asio::ip::tcp::socket socket,
                logging::Logger& logger,
                database::DatabaseManager& db);

    void start();

private:
    void read_line();
    void write_reply(const Pop3Reply& reply);
    void close();

    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf input_;
    logging::Logger& logger_;
    Pop3StateMachine state_machine_;
};

class Pop3Server {
public:
    Pop3Server(boost::asio::io_context& io_context,
               const config::ServerConfig& config,
               logging::Logger& logger,
               database::DatabaseManager& db);

    void start();

private:
    void accept_next();

    boost::asio::ip::tcp::acceptor acceptor_;
    config::ServerConfig config_;
    logging::Logger& logger_;
    database::DatabaseManager& db_;
};

} // namespace mailforge::pop3
