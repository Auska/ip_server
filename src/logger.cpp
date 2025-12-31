#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <queue>
#include <thread>
#include <condition_variable>

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

    // Send to async logger
    get_async_logger().log(log_line.str());
}

} // namespace ip_server