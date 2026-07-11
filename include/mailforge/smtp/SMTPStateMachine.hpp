#pragma once

#include "mailforge/smtp/SMTPCommand.hpp"

#include <string>
#include <vector>

namespace mailforge::database {
class DatabaseManager;
}

namespace mailforge::smtp {

struct SMTPReply {
    int code{500};
    std::string message;
    bool close_connection{false};
};

class SMTPStateMachine {
public:
    explicit SMTPStateMachine(std::string server_name, database::DatabaseManager* db = nullptr);

    [[nodiscard]] SMTPReply greeting() const;
    [[nodiscard]] SMTPReply handle(const SMTPCommand& command);
    [[nodiscard]] bool is_receiving_data() const;
    [[nodiscard]] bool is_authenticated() const;
    [[nodiscard]] const std::string& sender() const;
    [[nodiscard]] const std::vector<std::string>& recipients() const;
    [[nodiscard]] SMTPReply complete_data();

private:
    enum class State {
        fresh,
        greeted,
        auth_login_username,
        auth_login_password,
        auth_plain_waiting,
        sender,
        recipient,
        receiving_data,
        closed
    };

    std::string server_name_;
    State state_{State::fresh};
    std::string sender_;
    std::vector<std::string> recipients_;
    database::DatabaseManager* db_{nullptr};
    std::string auth_username_;
    bool authenticated_{false};
};

} // namespace mailforge::smtp
