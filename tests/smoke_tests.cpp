#include "mailforge/config/ConfigManager.hpp"
#include "mailforge/mime/MailMessage.hpp"
#include "mailforge/storage/MailStorage.hpp"
#include "mailforge/smtp/SMTPParser.hpp"
#include "mailforge/smtp/SMTPStateMachine.hpp"
#include "mailforge/database/DatabaseManager.hpp"
#include "mailforge/queue/QueueManager.hpp"
#include "mailforge/logging/Logger.hpp"
#include "mailforge/pop3/Pop3StateMachine.hpp"
#include "mailforge/mime/MimeParser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

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

    // DatabaseManager smoke tests
    const auto db_path = std::filesystem::temp_directory_path() / "mailforge-test.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
    {
        mailforge::database::DatabaseManager db(db_path);
        assert(db.register_user("alice", "password123"));
        assert(!db.register_user("alice", "another_pass")); // Duplicate username
        assert(db.verify_user("alice", "password123"));
        assert(!db.verify_user("alice", "wrong_pass"));
        assert(!db.verify_user("nonexistent", "password123"));

        auto msg_id = db.store_message("alice@example.com", "bob@example.com", "Hello", "Body content", "delivered");
        assert(msg_id > 0);
        assert(db.store_attachment(msg_id, "file.txt", 1024, "/path/to/file.txt"));

        // SMTP State Machine Authentication tests
        mailforge::smtp::SMTPStateMachine auth_machine("mailforge.local", &db);
        assert(auth_machine.handle(parser.parse_line("EHLO client\r\n")).code == 250);
        
        // AUTH PLAIN with inline credentials (base64 of "\0alice\0password123")
        auto plain_reply = auth_machine.handle(parser.parse_line("AUTH PLAIN AGFsaWNlAHBhc3N3b3JkMTIz\r\n"));
        assert(plain_reply.code == 235);
        assert(auth_machine.is_authenticated());

        // Test AUTH LOGIN
        mailforge::smtp::SMTPStateMachine login_machine("mailforge.local", &db);
        assert(login_machine.handle(parser.parse_line("EHLO client\r\n")).code == 250);
        
        // AUTH LOGIN initiation (returns Username challenge)
        assert(login_machine.handle(parser.parse_line("AUTH LOGIN\r\n")).code == 334);
        // Send base64 username ("alice" -> "YWxpY2U=")
        assert(login_machine.handle(parser.parse_line("YWxpY2U=\r\n")).code == 334);
        // Send base64 password ("password123" -> "cGFzc3dvcmQxMjM=")
        assert(login_machine.handle(parser.parse_line("cGFzc3dvcmQxMjM=\r\n")).code == 235);
        assert(login_machine.is_authenticated());

        // Test STARTTLS
        mailforge::smtp::SMTPStateMachine tls_machine("mailforge.local", &db);
        assert(tls_machine.handle(parser.parse_line("STARTTLS\r\n")).code == 503); // HELO/EHLO first
        assert(tls_machine.handle(parser.parse_line("EHLO client\r\n")).code == 250);
        auto tls_reply = tls_machine.handle(parser.parse_line("STARTTLS\r\n"));
        assert(tls_reply.code == 220);
        assert(tls_reply.message == "Ready to start TLS");

        // Test QueueManager
        mailforge::logging::Logger logger(mailforge::logging::Level::debug);
        auto pending_id = db.store_message("sender@example.com", "recipient@localhost", "Test Queue", "Body text", "pending");
        assert(pending_id > 0);
        
        auto pending_list = db.get_pending_messages();
        assert(!pending_list.empty());
        assert(pending_list.front().id == pending_id);
        
        boost::asio::io_context io_context;
        std::thread io_thread([&io_context] {
            boost::asio::io_context::work work(io_context);
            io_context.run();
        });

        mailforge::queue::QueueManager queue_manager(db, io_context, logger, 1);
        queue_manager.start();
        queue_manager.notify();
        
        // Wait a short moment for background delivery thread to run
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        auto user_msgs = db.get_user_messages("recipient@localhost");
        assert(!user_msgs.empty());
        assert(user_msgs.front().attempts == 3);
        
        queue_manager.stop();
        io_context.stop();
        if (io_thread.joinable()) {
            io_thread.join();
        }

        // Test POP3 State Machine
        mailforge::pop3::Pop3StateMachine pop3_machine(db);
        assert(pop3_machine.greeting().ok);
        
        // Try invalid flow
        assert(!pop3_machine.handle("STAT\r\n").ok); // Must USER first
        
        // Enter username
        auto user_reply = pop3_machine.handle("USER alice\r\n");
        assert(user_reply.ok);
        
        // Enter password
        auto pass_reply = pop3_machine.handle("PASS password123\r\n");
        assert(pass_reply.ok);
        
        // Should have 0 messages initially
        auto stat_reply = pop3_machine.handle("STAT\r\n");
        assert(stat_reply.ok);
        
        // Store a message for alice
        auto alice_msg_id = db.store_message("bob@example.com", "alice", "Hello Alice", "This is the body content", "delivered");
        assert(alice_msg_id > 0);
        
        // Relogin to load new messages
        mailforge::pop3::Pop3StateMachine pop3_session2(db);
        assert(pop3_session2.handle("USER alice\r\n").ok);
        assert(pop3_session2.handle("PASS password123\r\n").ok);
        
        // STAT should now report 1 message
        stat_reply = pop3_session2.handle("STAT\r\n");
        assert(stat_reply.ok);
        assert(stat_reply.message.find(" 1 ") != std::string::npos);
        
        // RETR message
        auto retr_reply = pop3_session2.handle("RETR 1\r\n");
        assert(retr_reply.ok);
        assert(retr_reply.message.find("This is the body content") != std::string::npos);
        
        // DELE message
        assert(pop3_session2.handle("DELE 1\r\n").ok);
        
        // STAT should report 0 active messages now
        assert(pop3_session2.handle("STAT\r\n").message.find(" 0 ") != std::string::npos);
        
        // QUIT to apply deletion
        assert(pop3_session2.handle("QUIT\r\n").ok);
        
        // Verify message is deleted in database
        assert(db.get_user_messages("alice").empty());

        // Test MimeParser
        std::string raw_mime = 
            "Content-Type: multipart/mixed; boundary=\"simple-boundary\"\r\n"
            "\r\n"
            "--simple-boundary\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "\r\n"
            "Hello, plain text world!\r\n"
            "--simple-boundary\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "\r\n"
            "<html><body>Hello html!</body></html>\r\n"
            "--simple-boundary\r\n"
            "Content-Type: text/plain; name=\"note.txt\"\r\n"
            "Content-Disposition: attachment; filename=\"note.txt\"\r\n"
            "Content-Transfer-Encoding: base64\r\n"
            "\r\n"
            "SGVsbG8gYXR0YWNobWVudCE=\r\n"
            "--simple-boundary--\r\n";
            
        mailforge::mime::MimeParser mime_parser(raw_mime);
        assert(mime_parser.text_body() == "Hello, plain text world!");
        assert(mime_parser.html_body() == "<html><body>Hello html!</body></html>");
        assert(!mime_parser.attachments().empty());
        assert(mime_parser.attachments().front().filename == "note.txt");
        assert(mime_parser.attachments().front().data == "Hello attachment!");
    }
    std::filesystem::remove(db_path);
}
