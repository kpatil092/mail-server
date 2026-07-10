#include "mailforge/smtp/SMTPStateMachine.hpp"

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

SMTPStateMachine::SMTPStateMachine(std::string server_name)
    : server_name_(std::move(server_name)) {}

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
    case CommandType::ehlo:
        if (!has_value(command.argument)) {
            return reply(501, "Domain argument required");
        }
        sender_.clear();
        recipients_.clear();
        state_ = State::greeted;
        return reply(250, server_name_ + " greets " + command.argument);
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
