#pragma once

#include <string>
#include <vector>

namespace mailforge::mime {

class MailMessage {
public:
    MailMessage(std::string sender, std::vector<std::string> recipients, std::string raw_content);

    [[nodiscard]] const std::string& sender() const;
    [[nodiscard]] const std::vector<std::string>& recipients() const;
    [[nodiscard]] const std::string& raw_content() const;

private:
    std::string sender_;
    std::vector<std::string> recipients_;
    std::string raw_content_;
};

} // namespace mailforge::mime
