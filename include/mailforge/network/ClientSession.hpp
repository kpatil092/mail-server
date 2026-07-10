#pragma once

#include "mailforge/logging/Logger.hpp"
#include "mailforge/storage/MailStorage.hpp"
#include "mailforge/smtp/SMTPParser.hpp"
#include "mailforge/smtp/SMTPStateMachine.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace mailforge::network {

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::ip::tcp::socket socket,
                  std::string server_name,
                  std::filesystem::path mail_directory,
                  logging::Logger& logger);

    void start();

private:
    void read_line();
    void write_reply(const smtp::SMTPReply& reply);
    void close();

    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf input_;
    smtp::SMTPParser parser_;
    std::string data_buffer_;
    storage::MailStorage storage_;
    smtp::SMTPStateMachine state_machine_;
    logging::Logger& logger_;
};

} // namespace mailforge::network
