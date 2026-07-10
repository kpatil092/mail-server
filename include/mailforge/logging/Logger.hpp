#pragma once

#include <mutex>
#include <string>

namespace mailforge::logging {

enum class Level {
    debug,
    info,
    warning,
    error
};

class Logger {
public:
    explicit Logger(Level minimum_level = Level::info);

    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

    static Level parse_level(const std::string& value);

private:
    void write(Level level, const std::string& message);

    Level minimum_level_;
    std::mutex mutex_;
};

} // namespace mailforge::logging
