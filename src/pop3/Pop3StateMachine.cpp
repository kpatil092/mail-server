#include "mailforge/pop3/Pop3StateMachine.hpp"
#include <sstream>
#include <algorithm>
#include <iostream>

namespace mailforge::pop3 {

Pop3StateMachine::Pop3StateMachine(database::DatabaseManager& db) : db_(db) {}

Pop3Reply Pop3StateMachine::greeting() const {
    return {true, "+OK POP3 MailForge Server ready"};
}

Pop3Reply Pop3StateMachine::handle(const std::string& command_line) {
    std::string line = command_line;
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    std::string arg;
    std::getline(iss, arg);
    // Trim leading whitespace from argument
    if (!arg.empty() && arg[0] == ' ') {
        arg.erase(0, 1);
    }

    if (state_ == State::closed) {
        return {false, "-ERR session closed", true};
    }

    if (cmd == "QUIT") {
        return handle_quit();
    }

    if (state_ == State::authorization_user) {
        if (cmd == "USER") {
            return handle_user(arg);
        }
        return {false, "-ERR Send USER command first"};
    }

    if (state_ == State::authorization_pass) {
        if (cmd == "PASS") {
            return handle_pass(arg);
        }
        return {false, "-ERR Send PASS command first"};
    }

    if (state_ == State::transaction) {
        if (cmd == "STAT") {
            return handle_stat();
        } else if (cmd == "LIST") {
            return handle_list(arg);
        } else if (cmd == "RETR") {
            return handle_retr(arg);
        } else if (cmd == "DELE") {
            return handle_dele(arg);
        } else if (cmd == "RSET") {
            return handle_rset();
        } else if (cmd == "NOOP") {
            return {true, "+OK"};
        }
    }

    return {false, "-ERR Unrecognized command"};
}

Pop3Reply Pop3StateMachine::handle_user(const std::string& arg) {
    if (arg.empty()) {
        return {false, "-ERR Username required"};
    }
    username_ = arg;
    state_ = State::authorization_pass;
    return {true, "+OK Send PASS next"};
}

Pop3Reply Pop3StateMachine::handle_pass(const std::string& arg) {
    if (db_.verify_user(username_, arg)) {
        state_ = State::transaction;
        session_messages_ = db_.get_user_messages(username_);
        deleted_ids_.clear();
        return {true, "+OK Welcome, mailbox has " + std::to_string(session_messages_.size()) + " message(s)"};
    }
    state_ = State::authorization_user;
    username_.clear();
    return {false, "-ERR Authentication failed"};
}

Pop3Reply Pop3StateMachine::handle_stat() {
    std::size_t active_count = 0;
    std::size_t total_size = 0;
    for (const auto& msg : session_messages_) {
        if (deleted_ids_.find(msg.id) == deleted_ids_.end()) {
            active_count++;
            total_size += msg.body.size() + 100; // approximation
        }
    }
    return {true, "+OK " + std::to_string(active_count) + " " + std::to_string(total_size)};
}

Pop3Reply Pop3StateMachine::handle_list(const std::string& arg) {
    if (!arg.empty()) {
        // Individual message listing
        try {
            int idx = std::stoi(arg) - 1;
            if (idx < 0 || idx >= static_cast<int>(session_messages_.size()) || 
                deleted_ids_.find(session_messages_[idx].id) != deleted_ids_.end()) {
                return {false, "-ERR No such message"};
            }
            return {true, "+OK " + std::to_string(idx + 1) + " " + 
                          std::to_string(session_messages_[idx].body.size() + 100)};
        } catch (...) {
            return {false, "-ERR Invalid message index"};
        }
    }

    // List all
    std::ostringstream oss;
    oss << "+OK " << session_messages_.size() << " messages\r\n";
    for (std::size_t i = 0; i < session_messages_.size(); ++i) {
        if (deleted_ids_.find(session_messages_[i].id) == deleted_ids_.end()) {
            oss << (i + 1) << " " << (session_messages_[i].body.size() + 100) << "\r\n";
        }
    }
    oss << ".";
    return {true, oss.str()};
}

Pop3Reply Pop3StateMachine::handle_retr(const std::string& arg) {
    try {
        int idx = std::stoi(arg) - 1;
        if (idx < 0 || idx >= static_cast<int>(session_messages_.size()) || 
            deleted_ids_.find(session_messages_[idx].id) != deleted_ids_.end()) {
            return {false, "-ERR No such message"};
        }

        const auto& msg = session_messages_[idx];
        std::ostringstream mail_data;
        mail_data << "From: " << msg.sender << "\r\n"
                  << "To: " << msg.recipient << "\r\n"
                  << "Subject: " << msg.subject << "\r\n"
                  << "\r\n"
                  << msg.body;

        std::string raw = mail_data.str();
        std::ostringstream reply;
        reply << "+OK " << raw.size() << " octets\r\n" << raw << "\r\n.";
        return {true, reply.str()};
    } catch (...) {
        return {false, "-ERR Invalid message index"};
    }
}

Pop3Reply Pop3StateMachine::handle_dele(const std::string& arg) {
    try {
        int idx = std::stoi(arg) - 1;
        if (idx < 0 || idx >= static_cast<int>(session_messages_.size()) || 
            deleted_ids_.find(session_messages_[idx].id) != deleted_ids_.end()) {
            return {false, "-ERR No such message"};
        }

        deleted_ids_.insert(session_messages_[idx].id);
        return {true, "+OK Message marked for deletion"};
    } catch (...) {
        return {false, "-ERR Invalid message index"};
    }
}

Pop3Reply Pop3StateMachine::handle_rset() {
    deleted_ids_.clear();
    return {true, "+OK All messages unmarked"};
}

Pop3Reply Pop3StateMachine::handle_quit() {
    if (state_ == State::transaction) {
        for (auto id : deleted_ids_) {
            try {
                db_.delete_message(id);
            } catch (...) {
                // Ignore DB error during deletion
            }
        }
    }
    state_ = State::closed;
    return {true, "+OK Goodbye", true};
}

} // namespace mailforge::pop3
