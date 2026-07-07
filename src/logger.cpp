#include "logger.h"

#include <spdlog/formatter.h>
#include <spdlog/pattern_formatter.h>

namespace ip_server {

// Custom formatter to map level names to match original logger
class CustomLevelFormatter : public spdlog::custom_flag_formatter {
   public:
    void format(const spdlog::details::log_msg& msg, const std::tm& /*tm_time*/,
                spdlog::memory_buf_t& dest) override {
        static constexpr const char* level_names[] = {
            "TRACE",    // trace
            "DEBUG",    // debug
            "INFO ",    // info
            "WARN ",    // warn
            "ERROR",    // err
            "CRITICAL", // critical
            "OFF",      // off
            "UNKNOWN"   // n_levels
        };
        size_t idx = msg.level < 8 ? static_cast<size_t>(msg.level) : 7;
        const char* level_name = level_names[idx];
        dest.append(level_name, level_name + std::char_traits<char>::length(level_name));
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return spdlog::details::make_unique<CustomLevelFormatter>();
    }
};

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

    auto make_formatter = [] {
        auto fmt = std::make_unique<spdlog::pattern_formatter>();
        fmt->add_flag<CustomLevelFormatter>('*').set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%*] %v");
        return fmt;
    };

    if (config_.enable_stdout) {
        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        stdout_sink->set_formatter(make_formatter());
        sinks.push_back(stdout_sink);
    }

    if (config_.enable_file_logging) {
        std::filesystem::path const log_dir = config_.log_file_path.parent_path();
        if (!log_dir.empty() && !std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
        }

        std::shared_ptr<spdlog::sinks::sink> file_sink;

        if (config_.rotation_type == RotationType::SIZE
            || config_.rotation_type == RotationType::BOTH) {
            file_sink =
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>(config_.log_file_path
                                                                           .string(),
                                                                       config_.max_file_size,
                                                                       config_.max_backup_files);
        } else if (config_.rotation_type == RotationType::TIME) {
            file_sink =
                std::make_shared<spdlog::sinks::daily_file_sink_mt>(config_.log_file_path.string(),
                                                                     0, 0);
        } else {
            file_sink = std::make_shared<
                spdlog::sinks::rotating_file_sink_mt>(config_.log_file_path.string(),
                                                      std::numeric_limits<size_t>::max(), 0);
        }

        file_sink->set_formatter(make_formatter());
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

}  // namespace ip_server