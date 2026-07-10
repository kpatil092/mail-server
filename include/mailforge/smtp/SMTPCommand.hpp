#pragma once

#include <string>

namespace mailforge::smtp {

enum class CommandType {
    helo,
    ehlo,
    mail_from,
    rcpt_to,
    data,
    quit,
    noop,
    rset,
    unknown
};

struct SMTPCommand {
    CommandType type{CommandType::unknown};
    std::string argument;
    std::string raw;
};

} // namespace mailforge::smtp
