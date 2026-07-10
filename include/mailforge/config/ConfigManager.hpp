#pragma once

#include "mailforge/config/ServerConfig.hpp"

#include <filesystem>

namespace mailforge::config {

class ConfigManager {
public:
    [[nodiscard]] ServerConfig load(const std::filesystem::path& path) const;
};

} // namespace mailforge::config
