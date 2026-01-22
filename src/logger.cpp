#include "logger.h"

#include <spdlog/formatter.h>
#include <spdlog/pattern_formatter.h>

namespace ip_server {

// Custom formatter to map level names to match original logger
class CustomLevelFormatter : public spdlog::custom_flag_formatter {
   public:
    void format(const spdlog::details::log_msg& msg, const std::tm&,
                spdlog::memory_buf_t& dest) override {
        const char* level_name = "";
        switch (msg.level) {
            case spdlog::level::trace:
                level_name = "TRACE";
                break;
            case spdlog::level::debug:
                level_name = "DEBUG";
                break;
            case spdlog::level::info:
                level_name = "INFO ";
                break;
            case spdlog::level::warn:
                level_name = "WARN ";
                break;
            case spdlog::level::err:
                level_name = "ERROR";
                break;
            case spdlog::level::critical:
                level_name = "CRITICAL";
                break;
            case spdlog::level::off:
                level_name = "OFF";
                break;
            case spdlog::level::n_levels:
                level_name = "UNKNOWN";
                break;
        }
        dest.append(level_name, level_name + std::strlen(level_name));
    }

    std::unique_ptr<custom_flag_formatter> clone() const override {
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

    // Create pattern formatter with custom level flag
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<CustomLevelFormatter>('*').set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%*] %v");

    // Add stdout sink
    if (config_.enable_stdout) {
        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        stdout_sink->set_formatter(formatter->clone());
        sinks.push_back(stdout_sink);
    }

    // Add file sink based on rotation type
    if (config_.enable_file_logging) {
        // Create log directory if it doesn't exist
        std::filesystem::path log_dir = config_.log_file_path.parent_path();
        if (!log_dir.empty() && !std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
        }

        std::shared_ptr<spdlog::sinks::sink> file_sink;

        if (config_.rotation_type == RotationType::SIZE
            || config_.rotation_type == RotationType::BOTH) {
            // Use rotating file sink for size-based rotation
            file_sink =
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>(config_.log_file_path
                                                                           .string(),
                                                                       config_.max_file_size,
                                                                       config_.max_backup_files);
        } else if (config_.rotation_type == RotationType::TIME) {
            // Use daily file sink for time-based rotation
            file_sink =
                std::make_shared<spdlog::sinks::daily_file_sink_mt>(config_.log_file_path.string(),
                                                                    0,  // Rotate at midnight
                                                                    0   // No additional offset
                );
        } else {
            // No rotation, use rotating sink with large max size
            file_sink = std::make_shared<
                spdlog::sinks::rotating_file_sink_mt>(config_.log_file_path.string(),
                                                      std::numeric_limits<size_t>::
                                                          max(),  // Very large size to effectively
                                                                  // disable rotation
                                                      0           // No backup files
            );
        }

        file_sink->set_formatter(formatter->clone());
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