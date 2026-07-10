#pragma once

#include "mailforge/smtp/SMTPCommand.hpp"

#include <string>
#include <vector>

namespace mailforge::smtp {

struct SMTPReply {
    int code{500};
    std::string message;
    bool close_connection{false};
};

class SMTPStateMachine {
public:
    explicit SMTPStateMachine(std::string server_name);

    [[nodiscard]] SMTPReply greeting() const;
    [[nodiscard]] SMTPReply handle(const SMTPCommand& command);
    [[nodiscard]] bool is_receiving_data() const;
    [[nodiscard]] const std::string& sender() const;
    [[nodiscard]] const std::vector<std::string>& recipients() const;
    [[nodiscard]] SMTPReply complete_data();

private:
    enum class State {
        fresh,
        greeted,
        sender,
        recipient,
        receiving_data,
        closed
    };

    std::string server_name_;
    State state_{State::fresh};
    std::string sender_;
    std::vector<std::string> recipients_;
};

} // namespace mailforge::smtp
