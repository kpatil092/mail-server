#pragma once

#include "mailforge/database/DatabaseManager.hpp"

#include <string>
#include <vector>
#include <set>

namespace mailforge::pop3 {

struct Pop3Reply {
    bool ok{false};
    std::string message;
    bool close_connection{false};
};

class Pop3StateMachine {
public:
    explicit Pop3StateMachine(database::DatabaseManager& db);

    Pop3Reply greeting() const;
    Pop3Reply handle(const std::string& command_line);

private:
    enum class State {
        authorization_user,
        authorization_pass,
        transaction,
        closed
    };

    Pop3Reply handle_user(const std::string& arg);
    Pop3Reply handle_pass(const std::string& arg);
    Pop3Reply handle_stat();
    Pop3Reply handle_list(const std::string& arg);
    Pop3Reply handle_retr(const std::string& arg);
    Pop3Reply handle_dele(const std::string& arg);
    Pop3Reply handle_rset();
    Pop3Reply handle_quit();

    database::DatabaseManager& db_;
    State state_{State::authorization_user};
    std::string username_;
    
    // Cache of active user messages for the current session
    std::vector<database::DatabaseManager::PendingMessage> session_messages_;
    std::set<std::int64_t> deleted_ids_;
};

} // namespace mailforge::pop3
