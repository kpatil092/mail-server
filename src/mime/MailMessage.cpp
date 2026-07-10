#include "mailforge/mime/MailMessage.hpp"

#include <utility>

namespace mailforge::mime {

MailMessage::MailMessage(std::string sender, std::vector<std::string> recipients, std::string raw_content)
    : sender_(std::move(sender)),
      recipients_(std::move(recipients)),
      raw_content_(std::move(raw_content)) {}

const std::string& MailMessage::sender() const {
    return sender_;
}

const std::vector<std::string>& MailMessage::recipients() const {
    return recipients_;
}

const std::string& MailMessage::raw_content() const {
    return raw_content_;
}

} // namespace mailforge::mime
