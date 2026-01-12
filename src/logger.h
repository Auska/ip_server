#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string>
#include <filesystem>
#include <chrono>

namespace ip_server {

enum class LogLevel {
    DEBUG = spdlog::level::debug,
    INFO = spdlog::level::info,
    WARNING = spdlog::level::warn,
    ERROR = spdlog::level::err
};

enum class RotationType {
    NONE,
    SIZE,
    TIME,
    BOTH
};

struct LogConfig {
    bool enable_file_logging = false;
    std::filesystem::path log_file_path = "logs/ip_server.log";
    RotationType rotation_type = RotationType::SIZE;
    size_t max_file_size = 10 * 1024 * 1024;  // 10 MB
    std::chrono::minutes rotation_interval = std::chrono::minutes(1440);  // 24 hours
    int max_backup_files = 5;
    bool enable_stdout = true;
};

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    void set_config(const LogConfig& config);
    
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

    void flush();

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const std::string& message);
    void setup_sinks();

    LogLevel level_ = LogLevel::INFO;
    LogConfig config_;
    std::shared_ptr<spdlog::logger> logger_;
};

// Keep existing macro interface unchanged
#define LOG_DEBUG(msg) ip_server::Logger::instance().debug(msg)
#define LOG_INFO(msg) ip_server::Logger::instance().info(msg)
#define LOG_WARNING(msg) ip_server::Logger::instance().warning(msg)
#define LOG_ERROR(msg) ip_server::Logger::instance().error(msg)

} // namespace ip_server