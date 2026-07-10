#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace mailforge::config {

struct ServerConfig {
    std::string host{"0.0.0.0"};
    std::uint16_t port{2525};
    std::size_t thread_pool{4};
    std::filesystem::path database{"mailforge.db"};
    std::filesystem::path mail_directory{"mail/"};
    std::string log_level{"info"};
    std::size_t max_connections{500};
    std::string server_name{"mailforge.local"};
};

} // namespace mailforge::config
