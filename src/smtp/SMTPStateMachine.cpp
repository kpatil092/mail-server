#include "mailforge/smtp/SMTPStateMachine.hpp"
#include "mailforge/utils/Base64.hpp"
#include "mailforge/database/DatabaseManager.hpp"

#include <utility>

namespace mailforge::smtp {
namespace {

SMTPReply reply(int code, std::string message, bool close = false) {
    return SMTPReply{code, std::move(message), close};
}

bool has_value(const std::string& value) {
    return !value.empty();
}

} // namespace

SMTPStateMachine::SMTPStateMachine(std::string server_name, database::DatabaseManager* db)
    : server_name_(std::move(server_name)), db_(db) {}

SMTPReply SMTPStateMachine::greeting() const {
    return reply(220, server_name_ + " MailForge SMTP ready");
}

SMTPReply SMTPStateMachine::handle(const SMTPCommand& command) {
    if (state_ == State::closed) {
        return reply(421, "Session already closed", true);
    }

    if (command.type == CommandType::quit) {
        state_ = State::closed;
        return reply(221, server_name_ + " closing connection", true);
    }

    if (state_ == State::auth_login_username) {
        if (command.type == CommandType::rset) {
            state_ = State::greeted;
            return reply(250, "Authentication aborted");
        }
        std::string username = utils::base64_decode(command.raw);
        if (username.empty() && !command.raw.empty() && command.raw != "=") {
            state_ = State::greeted;
            return reply(501, "Invalid base64 encoding");
        }
        auth_username_ = username;
        state_ = State::auth_login_password;
        return reply(334, "UGFzc3dvcmQ6"); // "Password:" in base64
    }

    if (state_ == State::auth_login_password) {
        if (command.type == CommandType::rset) {
            state_ = State::greeted;
            return reply(250, "Authentication aborted");
        }
        std::string password = utils::base64_decode(command.raw);
        if (db_ && db_->verify_user(auth_username_, password)) {
            authenticated_ = true;
            state_ = State::greeted;
            return reply(235, "Authentication successful");
        } else {
            state_ = State::greeted;
            return reply(535, "Authentication credentials invalid");
        }
    }

    if (state_ == State::auth_plain_waiting) {
        if (command.type == CommandType::rset) {
            state_ = State::greeted;
            return reply(250, "Authentication aborted");
        }
        std::string decoded = utils::base64_decode(command.raw);
        std::size_t first_null = decoded.find('\0');
        if (first_null == std::string::npos) {
            state_ = State::greeted;
            return reply(501, "Invalid PLAIN credentials format");
        }
        std::size_t second_null = decoded.find('\0', first_null + 1);
        if (second_null == std::string::npos) {
            state_ = State::greeted;
            return reply(501, "Invalid PLAIN credentials format");
        }
        std::string username = decoded.substr(first_null + 1, second_null - first_null - 1);
        std::string password = decoded.substr(second_null + 1);

        if (db_ && db_->verify_user(username, password)) {
            authenticated_ = true;
            state_ = State::greeted;
            return reply(235, "Authentication successful");
        } else {
            state_ = State::greeted;
            return reply(535, "Authentication credentials invalid");
        }
    }

    if (command.type == CommandType::noop) {
        return reply(250, "OK");
    }

    if (command.type == CommandType::rset) {
        if (state_ == State::fresh) {
            return reply(503, "Send HELO/EHLO first");
        }
        sender_.clear();
        recipients_.clear();
        state_ = State::greeted;
        return reply(250, "OK");
    }

    switch (command.type) {
    case CommandType::helo:
        if (!has_value(command.argument)) {
            return reply(501, "Domain argument required");
        }
        sender_.clear();
        recipients_.clear();
        state_ = State::greeted;
        return reply(250, server_name_ + " greets " + command.argument);
    case CommandType::ehlo:
        if (!has_value(command.argument)) {
            return reply(501, "Domain argument required");
        }
        sender_.clear();
        recipients_.clear();
        state_ = State::greeted;
        return reply(250, server_name_ + " greets " + command.argument + "\nAUTH PLAIN LOGIN\nSTARTTLS\nHELP");
    case CommandType::auth: {
        if (state_ != State::greeted) {
            return reply(503, "Already authenticated or wrong state");
        }
        if (authenticated_) {
            return reply(503, "Already authenticated");
        }
        std::string arg = command.argument;
        while (!arg.empty() && arg.front() == ' ') arg.erase(arg.begin());
        while (!arg.empty() && arg.back() == ' ') arg.pop_back();

        if (arg.empty()) {
            return reply(504, "Authentication mechanism required");
        }

        if (arg == "LOGIN" || arg == "login" || arg == "Login") {
            state_ = State::auth_login_username;
            return reply(334, "VXNlcm5hbWU6"); // "Username:" in base64
        } else if (arg.rfind("PLAIN", 0) == 0 || arg.rfind("plain", 0) == 0 || arg.rfind("Plain", 0) == 0) {
            std::string creds;
            if (arg.size() > 5) {
                creds = arg.substr(5);
                while (!creds.empty() && creds.front() == ' ') creds.erase(creds.begin());
            }
            if (creds.empty()) {
                state_ = State::auth_plain_waiting;
                return reply(334, "");
            } else {
                std::string decoded = utils::base64_decode(creds);
                std::size_t first_null = decoded.find('\0');
                if (first_null == std::string::npos) {
                    return reply(501, "Invalid PLAIN credentials format");
                }
                std::size_t second_null = decoded.find('\0', first_null + 1);
                if (second_null == std::string::npos) {
                    return reply(501, "Invalid PLAIN credentials format");
                }
                std::string username = decoded.substr(first_null + 1, second_null - first_null - 1);
                std::string password = decoded.substr(second_null + 1);

                if (db_ && db_->verify_user(username, password)) {
                    authenticated_ = true;
                    return reply(235, "Authentication successful");
                } else {
                    return reply(535, "Authentication credentials invalid");
                }
            }
        } else {
            return reply(504, "Unrecognized authentication mechanism");
        }
    }
    case CommandType::mail_from:
        if (state_ != State::greeted && state_ != State::sender && state_ != State::recipient) {
            return reply(503, "Send HELO/EHLO first");
        }
        if (!has_value(command.argument)) {
            return reply(501, "Sender required");
        }
        sender_ = command.argument;
        recipients_.clear();
        state_ = State::sender;
        return reply(250, "Sender OK");
    case CommandType::rcpt_to:
        if (state_ != State::sender && state_ != State::recipient) {
            return reply(503, "Need MAIL FROM before RCPT TO");
        }
        if (!has_value(command.argument)) {
            return reply(501, "Recipient required");
        }
        recipients_.push_back(command.argument);
        state_ = State::recipient;
        return reply(250, "Recipient OK");
    case CommandType::data:
        if (state_ != State::recipient || recipients_.empty()) {
            return reply(503, "Need at least one recipient before DATA");
        }
        state_ = State::receiving_data;
        return reply(354, "End data with <CR><LF>.<CR><LF>");
    case CommandType::starttls:
        if (state_ == State::fresh) {
            return reply(503, "Send HELO/EHLO first");
        }
        sender_.clear();
        recipients_.clear();
        state_ = State::fresh;
        authenticated_ = false;
        return reply(220, "Ready to start TLS");
    case CommandType::unknown:
        return reply(500, "Command unrecognized");
    case CommandType::quit:
    case CommandType::noop:
    case CommandType::rset:
        break;
    }

    return reply(500, "Command unrecognized");
}

bool SMTPStateMachine::is_receiving_data() const {
    return state_ == State::receiving_data;
}

bool SMTPStateMachine::is_authenticated() const {
    return authenticated_;
}

const std::string& SMTPStateMachine::sender() const {
    return sender_;
}

const std::vector<std::string>& SMTPStateMachine::recipients() const {
    return recipients_;
}

SMTPReply SMTPStateMachine::complete_data() {
    if (state_ != State::receiving_data) {
        return reply(503, "DATA not in progress");
    }

    sender_.clear();
    recipients_.clear();
    state_ = State::greeted;
    return reply(250, "Message accepted for delivery");
}

} // namespace mailforge::smtp
