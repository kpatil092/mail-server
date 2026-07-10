#include "mailforge/config/ConfigManager.hpp"
#include "mailforge/mime/MailMessage.hpp"
#include "mailforge/storage/MailStorage.hpp"
#include "mailforge/smtp/SMTPParser.hpp"
#include "mailforge/smtp/SMTPStateMachine.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

bool file_contains(const std::filesystem::path& root, const std::string& text) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::ifstream input(entry.path());
        const std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (content.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    mailforge::config::ConfigManager config_manager;
    const auto config = config_manager.load("config/mailforge.json");
    assert(config.port == 2525);
    assert(config.server_name == "mailforge.local");

    mailforge::smtp::SMTPParser parser;
    const auto mail_from = parser.parse_line("MAIL FROM:<alice@example.com>\r\n");
    assert(mail_from.type == mailforge::smtp::CommandType::mail_from);
    assert(mail_from.argument == "<alice@example.com>");

    mailforge::smtp::SMTPStateMachine machine(config.server_name);
    assert(machine.greeting().code == 220);
    assert(machine.handle(parser.parse_line("RSET\r\n")).code == 503);
    assert(machine.handle(parser.parse_line("EHLO client.example\r\n")).code == 250);
    assert(machine.handle(mail_from).code == 250);
    assert(machine.handle(parser.parse_line("RCPT TO:<bob@example.com>\r\n")).code == 250);
    assert(machine.handle(parser.parse_line("DATA\r\n")).code == 354);
    assert(machine.is_receiving_data());
    assert(machine.complete_data().code == 250);
    assert(machine.handle(parser.parse_line("QUIT\r\n")).close_connection);

    assert(std::filesystem::exists("config/mailforge.json"));

    const auto storage_root = std::filesystem::temp_directory_path() / "mailforge-storage-smoke";
    mailforge::storage::MailStorage storage(storage_root);
    storage.store(mailforge::mime::MailMessage("<alice@example.com>", {"<bob@example.com>"}, "Subject: Test\n\nHello\n"));
    assert(std::filesystem::exists(storage_root / "bob@example.com" / "inbox"));
    assert(file_contains(storage_root / "bob@example.com" / "inbox", "X-MailForge-From: alice@example.com"));
    assert(file_contains(storage_root / "bob@example.com" / "inbox", "Subject: Test"));
}
