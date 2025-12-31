#pragma once

#include <iostream>
#include <string>
#include <mutex>

namespace ip_server {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level) { level_ = level; }

    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

private:
    Logger() = default;
    void log(LogLevel level, const std::string& message);

    LogLevel level_ = LogLevel::INFO;
    std::mutex mutex_;
};

#define LOG_DEBUG(msg) ip_server::Logger::instance().debug(msg)
#define LOG_INFO(msg) ip_server::Logger::instance().info(msg)
#define LOG_WARNING(msg) ip_server::Logger::instance().warning(msg)
#define LOG_ERROR(msg) ip_server::Logger::instance().error(msg)

} // namespace ip_server