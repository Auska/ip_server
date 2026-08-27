#include "logger.h"

#include <spdlog/formatter.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

namespace ip_server {

namespace {

// Uppercase level name flag: spdlog has no built-in full-word uppercase level.
class UpperLevelFormatter : public spdlog::custom_flag_formatter {
   public:
    void format(const spdlog::details::log_msg& msg, const std::tm&,
                spdlog::memory_buf_t& dest) override {
        auto level = spdlog::level::to_string_view(msg.level);
        std::transform(level.begin(), level.end(), std::back_inserter(dest),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return spdlog::details::make_unique<UpperLevelFormatter>();
    }
};

}  // namespace

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    setup_sinks();
}

Logger::~Logger() {
    if (logger_) {
        logger_->flush();
    }
    spdlog::shutdown();
}

void Logger::setup_sinks() {
    std::vector<spdlog::sink_ptr> sinks;

    auto makeFormatter = [] {
        auto fmt = std::make_unique<spdlog::pattern_formatter>();
        fmt->add_flag<UpperLevelFormatter>('*').set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%*] %v");
        return fmt;
    };

    if (config_.enable_stdout_) {
        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        stdout_sink->set_formatter(makeFormatter());
        sinks.push_back(stdout_sink);
    }

    if (config_.enable_file_logging_) {
        std::filesystem::path const log_dir = config_.log_file_path_.parent_path();
        if (!log_dir.empty() && !std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
        }

        std::shared_ptr<spdlog::sinks::sink> file_sink;

        if (config_.rotation_type_ == RotationType::SIZE) {
            file_sink =
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>(config_.log_file_path_
                                                                           .string(),
                                                                       config_.max_file_size_,
                                                                       config_.max_backup_files_);
        } else {
            file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                config_.log_file_path_.string());
        }

        file_sink->set_formatter(makeFormatter());
        sinks.push_back(file_sink);
    }

    // Create logger with combined sinks
    logger_ = std::make_shared<spdlog::logger>("ip_server", sinks.begin(), sinks.end());
    logger_->set_level(static_cast<spdlog::level::level_enum>(level_));
    logger_->flush_on(spdlog::level::err);

    // Register as default logger
    spdlog::set_default_logger(logger_);
}

void Logger::set_level(LogLevel level) {
    level_ = level;
    if (logger_) {
        logger_->set_level(static_cast<spdlog::level::level_enum>(level));
    }
}

void Logger::set_config(const LogConfig& config) {
    config_ = config;
    setup_sinks();  // Reconfigure sinks
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::flush() {
    if (logger_) {
        logger_->flush();
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (!logger_) {
        return;
    }

    auto spdlog_level = static_cast<spdlog::level::level_enum>(level);
    logger_->log(spdlog_level, message);
}

void Logger::signal_safe_log(const char* signal_name) const {
    // Only async-signal-safe operations: write(2) to stderr.
    static constexpr char prefix[] = "[SIGNAL] Received ";
    static constexpr char suffix[] = ", shutting down gracefully...\n";
    // write is async-signal-safe per POSIX.
    ::write(STDERR_FILENO, prefix, sizeof(prefix) - 1);
    ::write(STDERR_FILENO, signal_name, std::strlen(signal_name));
    ::write(STDERR_FILENO, suffix, sizeof(suffix) - 1);
}

}  // namespace ip_server