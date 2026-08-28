#include "logger.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <vector>

namespace ip_server {

void init_logging(const LogConfig& config) {
    std::vector<spdlog::sink_ptr> sinks;

    if (config.enable_stdout_) {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    if (config.enable_file_logging_) {
        std::filesystem::path const log_dir =
            std::filesystem::path(config.log_file_path_).parent_path();
        if (!log_dir.empty() && !std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
        }

        if (config.rotation_type_ == "size") {
            sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config.log_file_path_, config.max_file_size_, config.max_backup_files_));
        } else {
            sinks.push_back(
                std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.log_file_path_));
        }
    }

    auto logger = std::make_shared<spdlog::logger>("ip_server", sinks.begin(), sinks.end());
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    logger->flush_on(spdlog::level::err);
    spdlog::set_default_logger(logger);
}

void set_log_level(const std::string& level) {
    spdlog::set_level(spdlog::level::from_str(level));
}

void signal_safe_log(const char* signal_name) {
    // Only async-signal-safe operations: write(2) to stderr.
    static constexpr char prefix[] = "[SIGNAL] Received ";
    static constexpr char suffix[] = ", shutting down gracefully...\n";
    ::write(STDERR_FILENO, prefix, sizeof(prefix) - 1);
    ::write(STDERR_FILENO, signal_name, std::strlen(signal_name));
    ::write(STDERR_FILENO, suffix, sizeof(suffix) - 1);
}

}  // namespace ip_server
