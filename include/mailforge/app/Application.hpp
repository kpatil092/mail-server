#pragma once

#include "mailforge/config/ServerConfig.hpp"

namespace mailforge::app {

class Application {
private:
    config::ServerConfig config_;
public:
    explicit Application(config::ServerConfig config);
    int run();
};

} 
// namespace mailforge::app
