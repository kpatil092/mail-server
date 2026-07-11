#pragma once

#include "mailforge/logging/Logger.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace mailforge::smtp {

class SmtpClient {
public:
    SmtpClient(boost::asio::io_context& io_context, logging::Logger& logger);

    void deliver(const std::string& sender,
                 const std::string& recipient,
                 const std::string& subject,
                 const std::string& body,
                 std::function<void(bool success)> callback);

private:
    std::vector<std::string> resolve_mx(const std::string& domain);
    void try_deliver_to_hosts(const std::vector<std::string>& hosts,
                              std::size_t host_index,
                              const std::string& sender,
                              const std::string& recipient,
                              const std::string& mail_data,
                              std::function<void(bool success)> callback);

    boost::asio::io_context& io_context_;
    logging::Logger& logger_;
};

} // namespace mailforge::smtp
