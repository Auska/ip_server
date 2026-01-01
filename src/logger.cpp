#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <queue>
#include <thread>
#include <condition_variable>
#include <algorithm>

namespace ip_server {

// Async logger implementation
class AsyncLogger {
public:
    AsyncLogger() : running_(true) {
        worker_thread_ = std::thread(&AsyncLogger::process_queue, this);
    }

    ~AsyncLogger() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
            cv_.notify_all();
        }
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(message);
        cv_.notify_one();
    }

private:
    void process_queue() {
        while (running_ || !queue_.empty()) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || !running_; });

            while (!queue_.empty()) {
                auto message = queue_.front();
                queue_.pop();
                lock.unlock();

                // Write to stdout without holding lock
                std::cout << message << std::endl;

                lock.lock();
            }
        }
    }

    std::queue<std::string> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_thread_;
    bool running_;
};

// Global async logger instance
static AsyncLogger& get_async_logger() {
    static AsyncLogger instance;
    return instance;
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
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
    if (file_stream_.is_open()) {
        file_stream_.flush();
    }
    std::cout.flush();
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < level_) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    const char* level_str = "";
    switch (level) {
        case LogLevel::DEBUG:   level_str = "DEBUG"; break;
        case LogLevel::INFO:    level_str = "INFO "; break;
        case LogLevel::WARNING: level_str = "WARN "; break;
        case LogLevel::ERROR:   level_str = "ERROR"; break;
    }

    std::ostringstream log_line;
    log_line << "[" << oss.str() << "] [" << level_str << "] " << message;

    std::string log_message = log_line.str();

    // Send to async logger for stdout
    if (config_.enable_stdout) {
        get_async_logger().log(log_message);
    }

    // Write to file if enabled
    if (config_.enable_file_logging) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Open file if not open
        if (!file_stream_.is_open()) {
            // Create log directory if it doesn't exist
            std::filesystem::path log_dir = config_.log_file_path.parent_path();
            if (!log_dir.empty() && !std::filesystem::exists(log_dir)) {
                std::filesystem::create_directories(log_dir);
            }

            file_stream_.open(config_.log_file_path, std::ios::app);
            if (!file_stream_.is_open()) {
                std::cerr << "Failed to open log file: " << config_.log_file_path << std::endl;
                config_.enable_file_logging = false;
                return;
            }

            last_rotation_time_ = std::chrono::system_clock::now();
            current_file_size_ = std::filesystem::exists(config_.log_file_path)
                ? std::filesystem::file_size(config_.log_file_path)
                : 0;
        }

        // Check if rotation is needed
        rotate_if_needed();

        // Write to file
        if (file_stream_.is_open()) {
            file_stream_ << log_message << std::endl;
            file_stream_.flush();
            current_file_size_ += log_message.length() + 1;  // +1 for newline
        }
    }
}

void Logger::rotate_if_needed() {
    if (!file_stream_.is_open()) {
        return;
    }

    bool need_rotation = false;

    // Check size-based rotation
    if (config_.rotation_type == RotationType::SIZE || 
        config_.rotation_type == RotationType::BOTH) {
        if (current_file_size_ >= config_.max_file_size) {
            need_rotation = true;
        }
    }

    // Check time-based rotation
    if (config_.rotation_type == RotationType::TIME || 
        config_.rotation_type == RotationType::BOTH) {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
            now - last_rotation_time_);
        if (elapsed >= config_.rotation_interval) {
            need_rotation = true;
        }
    }

    if (need_rotation) {
        if (config_.rotation_type == RotationType::SIZE || 
            config_.rotation_type == RotationType::BOTH) {
            rotate_by_size();
        } else {
            rotate_by_time();
        }

        last_rotation_time_ = std::chrono::system_clock::now();
        current_file_size_ = 0;
    }
}

void Logger::rotate_by_size() {
    // Close current file
    if (file_stream_.is_open()) {
        file_stream_.close();
    }

    // Rotate backup files
    for (int i = config_.max_backup_files - 1; i >= 1; i--) {
        std::filesystem::path old_backup = get_backup_filename(i);
        std::filesystem::path new_backup = get_backup_filename(i + 1);

        if (std::filesystem::exists(old_backup)) {
            if (i + 1 > config_.max_backup_files) {
                std::filesystem::remove(old_backup);
            } else {
                std::filesystem::rename(old_backup, new_backup);
            }
        }
    }

    // Rename current file to .1
    if (std::filesystem::exists(config_.log_file_path)) {
        std::filesystem::path backup = get_backup_filename(1);
        std::filesystem::rename(config_.log_file_path, backup);
    }

    // Open new file
    file_stream_.open(config_.log_file_path, std::ios::app);
}

void Logger::rotate_by_time() {
    // Close current file
    if (file_stream_.is_open()) {
        file_stream_.close();
    }

    // Create timestamped backup
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");

    std::filesystem::path backup_path = config_.log_file_path;
    backup_path += "." + oss.str();

    if (std::filesystem::exists(config_.log_file_path)) {
        std::filesystem::rename(config_.log_file_path, backup_path);
    }

    // Clean up old backups (keep only max_backup_files)
    std::vector<std::filesystem::path> backup_files;
    std::filesystem::path log_dir = config_.log_file_path.parent_path();
    std::string log_name = config_.log_file_path.filename().string();

    if (std::filesystem::exists(log_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(log_dir)) {
            if (entry.path().string().find(log_name) == 0) {
                backup_files.push_back(entry.path());
            }
        }
    }

    // Sort by modification time (oldest first)
    std::sort(backup_files.begin(), backup_files.end(),
        [](const std::filesystem::path& a, const std::filesystem::path& b) {
            return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
        });

    // Remove excess backups
    while (backup_files.size() > static_cast<size_t>(config_.max_backup_files)) {
        std::filesystem::remove(backup_files[0]);
        backup_files.erase(backup_files.begin());
    }

    // Open new file
    file_stream_.open(config_.log_file_path, std::ios::app);
}

std::string Logger::get_backup_filename(int index) {
    std::filesystem::path backup = config_.log_file_path;
    backup += "." + std::to_string(index);
    return backup.string();
}

} // namespace ip_server