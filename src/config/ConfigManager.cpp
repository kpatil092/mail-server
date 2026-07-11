#include "mailforge/config/ConfigManager.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace mailforge::config {
namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open config file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string string_value(const std::string& document, const std::string& key, const std::string& fallback) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    return std::regex_search(document, match, pattern) ? match[1].str() : fallback;
}

std::size_t number_value(const std::string& document, const std::string& key, std::size_t fallback) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    return std::regex_search(document, match, pattern) ? static_cast<std::size_t>(std::stoull(match[1].str())) : fallback;
}

} // namespace

ServerConfig ConfigManager::load(const std::filesystem::path& path) const {
    const auto document = read_file(path);

    ServerConfig config;
    config.host = string_value(document, "host", config.host);
    config.port = static_cast<std::uint16_t>(number_value(document, "port", config.port));
    config.thread_pool = number_value(document, "thread_pool", config.thread_pool);
    config.database = string_value(document, "database", config.database.string());
    config.mail_directory = string_value(document, "mail_directory", config.mail_directory.string());
    config.log_level = string_value(document, "log_level", config.log_level);
    config.max_connections = number_value(document, "max_connections", config.max_connections);
    config.server_name = string_value(document, "server_name", config.server_name);
    config.tls_certificate = string_value(document, "tls_certificate", config.tls_certificate);
    config.tls_private_key = string_value(document, "tls_private_key", config.tls_private_key);
    config.pop3_port = static_cast<std::uint16_t>(number_value(document, "pop3_port", config.pop3_port));

    if (config.port == 0) {
        throw std::runtime_error("Config value 'port' must be between 1 and 65535");
    }
    if (config.thread_pool == 0) {
        throw std::runtime_error("Config value 'thread_pool' must be at least 1");
    }
    if (config.max_connections == 0) {
        throw std::runtime_error("Config value 'max_connections' must be at least 1");
    }
    if (config.pop3_port == 0) {
        throw std::runtime_error("Config value 'pop3_port' must be between 1 and 65535");
    }

    return config;
}

} // namespace mailforge::config
