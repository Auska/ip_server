#pragma once

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <string>

namespace ip_server {

struct LogConfig {
    bool enable_file_logging_ = false;
    std::string log_file_path_ = "logs/ip_server.log";
    std::string rotation_type_ = "size";  // "none" or "size"
    size_t max_file_size_      = 10 * 1024 * 1024;
    int max_backup_files_      = 5;
    bool enable_stdout_        = true;
};

/// Configure the default spdlog logger. Call once at startup.
void init_logging(const LogConfig& config);

/// Set level by spdlog name: trace, debug, info, warning, err, critical, off.
void set_log_level(const std::string& level);

/// Async-signal-safe log (write(2) to stderr only). Signal handlers only.
void signal_safe_log(const char* signal_name);

#define LOG_DEBUG(msg) spdlog::debug(msg)
#define LOG_INFO(msg) spdlog::info(msg)
#define LOG_WARNING(msg) spdlog::warn(msg)
#define LOG_ERROR(msg) spdlog::error(msg)

}  // namespace ip_server
