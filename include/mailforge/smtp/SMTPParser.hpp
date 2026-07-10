#pragma once

#include "mailforge/smtp/SMTPCommand.hpp"

#include <string_view>

namespace mailforge::smtp {

class SMTPParser {
public:
    [[nodiscard]] SMTPCommand parse_line(std::string_view line) const;
};

} // namespace mailforge::smtp
