#pragma once

#include "mailforge/config/ServerConfig.hpp"

namespace mailforge::app {

class Application {
public:
    explicit Application(config::ServerConfig config);

    int run();

private:
    config::ServerConfig config_;
};

} // namespace mailforge::app
