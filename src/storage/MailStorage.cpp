#include "mailforge/storage/MailStorage.hpp"

#include <chrono>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace mailforge::storage {
namespace {

std::string trim_address(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    if (value.size() >= 2 && value.front() == '<' && value.back() == '>') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

std::string mailbox_name(std::string value) {
    value = trim_address(std::move(value));
    if (value.empty()) {
        throw std::runtime_error("Mailbox address cannot be empty");
    }

    for (auto& ch : value) {
        const auto c = static_cast<unsigned char>(ch);
        if (!std::isalnum(c) && ch != '@' && ch != '.' && ch != '_' && ch != '-') {
            ch = '_';
        }
    }
    return value;
}

std::string message_id() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

} // namespace

MailStorage::MailStorage(std::filesystem::path root)
    : root_(std::move(root)) {}

void MailStorage::store(const mime::MailMessage& message) const {
    const auto id = message_id();
    const auto sender = trim_address(message.sender());
    if (sender.empty()) {
        throw std::runtime_error("Sender address cannot be empty");
    }

    for (const auto& recipient : message.recipients()) {
        const auto mailbox = mailbox_name(recipient);
        const auto inbox = root_ / mailbox / "inbox";
        std::filesystem::create_directories(inbox);

        const auto path = inbox / (id + ".eml");
        std::ofstream output(path);
        if (!output) {
            throw std::runtime_error("Unable to write message: " + path.string());
        }

        output << "X-MailForge-Message-Id: " << id << "\n";
        output << "Received: by MailForge\n";
        output << "X-MailForge-From: " << sender << "\n";
        output << "X-MailForge-To: " << mailbox << "\n";
        output << message.raw_content();
    }
}

} // namespace mailforge::storage
