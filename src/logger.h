#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <filesystem>
#include <chrono>

namespace ip_server {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
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

    void set_level(LogLevel level) { level_ = level; }
    void set_config(const LogConfig& config) { 
        std::lock_guard<std::mutex> lock(mutex_);
        // Close current file if logging is disabled or path changed
        if (file_stream_.is_open()) {
            file_stream_.close();
        }
        config_ = config; 
    }

    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

    void flush();

private:
    Logger() = default;
    ~Logger();
    void log(LogLevel level, const std::string& message);
    void rotate_if_needed();
    void rotate_by_size();
    void rotate_by_time();
    std::string get_backup_filename(int index);

    LogLevel level_ = LogLevel::INFO;
    LogConfig config_;
    std::mutex mutex_;
    std::ofstream file_stream_;
    std::chrono::system_clock::time_point last_rotation_time_;
    size_t current_file_size_ = 0;
};

#define LOG_DEBUG(msg) ip_server::Logger::instance().debug(msg)
#define LOG_INFO(msg) ip_server::Logger::instance().info(msg)
#define LOG_WARNING(msg) ip_server::Logger::instance().warning(msg)
#define LOG_ERROR(msg) ip_server::Logger::instance().error(msg)

} // namespace ip_server