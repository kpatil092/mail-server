#include "mailforge/logging/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <format>

namespace mailforge::logging {
namespace {

std::string_view name_for(Level level) {
    switch (level) {
    case Level::debug:
        return "debug";
    case Level::info:
        return "info";
    case Level::warning:
        return "warning";
    case Level::error:
        return "error";
    }
    return "info";
}

} // namespace

Logger::Logger(Level minimum_level)
    : minimum_level_(minimum_level) {}

void Logger::debug(const std::string& message) {
    write(Level::debug, message);
}

void Logger::info(const std::string& message) {
    write(Level::info, message);
}

void Logger::warning(const std::string& message) {
    write(Level::warning, message);
}

void Logger::error(const std::string& message) {
    write(Level::error, message);
}

Level Logger::parse_level(const std::string& value) {
    auto normalized = value;
    std::ranges::transform(normalized, normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (normalized == "debug") {
        return Level::debug;
    }
    if (normalized == "warning" || normalized == "warn") {
        return Level::warning;
    }
    if (normalized == "error") {
        return Level::error;
    }
    return Level::info;
}

void Logger::write(Level level, const std::string& message) {
    if (static_cast<int>(level) < static_cast<int>(minimum_level_)) {
        return;
    }

    const auto now = std::chrono::system_clock::now();

    std::lock_guard lock(mutex_);
    std::cout << std::format("[{:%Y-%m-%d %H:%M:%S}] ", now)
              << "[" << name_for(level) << "] " << message << '\n';
}

} // namespace mailforge::logging
