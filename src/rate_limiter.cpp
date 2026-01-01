#include "rate_limiter.h"
#include "logger.h"
#include <algorithm>

namespace ip_server {

bool RateLimiter::is_allowed(const std::string& ip_address) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto& record = ip_records_[ip_address];

    // Remove timestamps outside the time window
    while (!record.timestamps.empty() && 
           now - record.timestamps.front() > window_) {
        record.timestamps.pop_front();
    }

    // Check if under limit
    if (record.timestamps.size() < static_cast<size_t>(max_requests_)) {
        record.timestamps.push_back(now);
        return true;
    }

    return false;
}

int RateLimiter::get_remaining(const std::string& ip_address) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = ip_records_.find(ip_address);
    if (it == ip_records_.end()) {
        return max_requests_;
    }

    auto now = std::chrono::steady_clock::now();
    size_t count = 0;

    // Count timestamps within the window
    for (const auto& timestamp : it->second.timestamps) {
        if (now - timestamp <= window_) {
            count++;
        }
    }

    return std::max(0, max_requests_ - static_cast<int>(count));
}

void RateLimiter::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();

    // Remove old entries for all IPs
    for (auto& [ip, record] : ip_records_) {
        while (!record.timestamps.empty() && 
               now - record.timestamps.front() > window_) {
            record.timestamps.pop_front();
        }
    }

    // Remove empty records
    auto it = ip_records_.begin();
    while (it != ip_records_.end()) {
        if (it->second.timestamps.empty()) {
            it = ip_records_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace ip_server