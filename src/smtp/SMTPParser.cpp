#include "mailforge/smtp/SMTPParser.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace mailforge::smtp {
namespace {

std::string trim_crlf(std::string_view value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
        value.remove_suffix(1);
    }
    return std::string(value);
}

std::string uppercase(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

bool starts_with_case_insensitive(const std::string& value, std::string_view prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }
    return uppercase(value.substr(0, prefix.size())) == prefix;
}

std::string rest_after(const std::string& value, std::size_t offset) {
    if (value.size() <= offset) {
        return {};
    }
    auto result = value.substr(offset);
    while (!result.empty() && result.front() == ' ') {
        result.erase(result.begin());
    }
    return result;
}

} // namespace

SMTPCommand SMTPParser::parse_line(std::string_view line) const {
    SMTPCommand command;
    command.raw = trim_crlf(line);

    const auto first_space = command.raw.find(' ');
    const auto verb = uppercase(command.raw.substr(0, first_space));
    command.argument = first_space == std::string::npos ? "" : rest_after(command.raw, first_space + 1);

    if (verb == "HELO") {
        command.type = CommandType::helo;
    } else if (verb == "EHLO") {
        command.type = CommandType::ehlo;
    } else if (starts_with_case_insensitive(command.raw, "MAIL FROM:")) {
        command.type = CommandType::mail_from;
        command.argument = rest_after(command.raw, 10);
    } else if (starts_with_case_insensitive(command.raw, "RCPT TO:")) {
        command.type = CommandType::rcpt_to;
        command.argument = rest_after(command.raw, 8);
    } else if (verb == "DATA") {
        command.type = CommandType::data;
    } else if (verb == "QUIT") {
        command.type = CommandType::quit;
    } else if (verb == "NOOP") {
        command.type = CommandType::noop;
    } else if (verb == "RSET") {
        command.type = CommandType::rset;
    } else if (verb == "AUTH") {
        command.type = CommandType::auth;
    } else if (verb == "STARTTLS") {
        command.type = CommandType::starttls;
    }

    return command;
}

} // namespace mailforge::smtp
