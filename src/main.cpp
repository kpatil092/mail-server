#include "mailforge/app/Application.hpp"
#include "mailforge/config/ConfigManager.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    const auto config_path = argc > 1 ? argv[1] : "config/mailforge.json";

    try {
        mailforge::config::ConfigManager config_manager;
        mailforge::app::Application app(config_manager.load(config_path));
        return app.run();
    } catch (const std::exception& error) {
        std::cerr << "MailForge failed to start: " << error.what() << '\n';
        return 1;
    }
}
