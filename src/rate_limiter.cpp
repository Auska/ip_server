#include "rate_limiter.h"

#include <algorithm>

#include "logger.h"

namespace ip_server {

void RateLimiter::cleanup_old_timestamps(IPRecord& record) const {
    auto now = std::chrono::steady_clock::now();
    while (!record.timestamps.empty() && now - record.timestamps.front() > window_) {
        record.timestamps.pop_front();
    }
}

bool RateLimiter::is_allowed(const std::string& ip_address) {
    std::lock_guard<std::mutex> lock(mutex_);

    total_requests_++;

    auto now     = std::chrono::steady_clock::now();
    auto& record = ip_records_[ip_address];

    // Update last access time
    record.last_access = now;

    // Remove timestamps outside the time window
    cleanup_old_timestamps(record);

    // Check if under limit
    if (record.timestamps.size() < static_cast<size_t>(max_requests_)) {
        record.timestamps.push_back(now);

        // Enforce maximum IP records limit (LRU eviction)
        if (ip_records_.size() > max_ip_records_) {
            // Find the least recently used IP (oldest last_access)
            auto lru_it = ip_records_.begin();
            for (auto it = ip_records_.begin(); it != ip_records_.end(); ++it) {
                if (it->second.last_access < lru_it->second.last_access) {
                    lru_it = it;
                }
            }

            LOG_DEBUG("Rate limiter: Evicting IP record (LRU): " + lru_it->first);
            ip_records_.erase(lru_it);
        }

        return true;
    }

    total_rate_limited_++;
    return false;
}

int RateLimiter::get_remaining(const std::string& ip_address) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = ip_records_.find(ip_address);
    if (it == ip_records_.end()) {
        return max_requests_;
    }

    auto now     = std::chrono::steady_clock::now();
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

    // Remove old entries for all IPs
    for (auto& [ip, record] : ip_records_) {
        cleanup_old_timestamps(record);
    }

    // Remove empty records
    auto it              = ip_records_.begin();
    size_t removed_count = 0;
    while (it != ip_records_.end()) {
        if (it->second.timestamps.empty()) {
            it = ip_records_.erase(it);
            removed_count++;
        } else {
            ++it;
        }
    }

    if (removed_count > 0) {
        LOG_DEBUG("Rate limiter: Cleaned up " + std::to_string(removed_count) + " idle IP records");
    }
}

size_t RateLimiter::estimate_memory_usage() const {
    size_t total = 0;

    // Estimate map overhead (rough approximation)
    total += ip_records_.size() * sizeof(std::pair<std::string, IPRecord>);

    // Estimate string storage for IP addresses
    for (const auto& [ip, record] : ip_records_) {
        total += ip.size() * sizeof(char);
        // Estimate deque overhead
        total += record.timestamps.size() * sizeof(std::chrono::steady_clock::time_point);
        total += sizeof(std::deque<std::chrono::steady_clock::time_point>);
    }

    return total;
}

RateLimiter::MemoryStats RateLimiter::get_memory_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    MemoryStats stats;
    stats.ip_record_count        = ip_records_.size();
    stats.total_timestamps       = 0;
    stats.estimated_memory_bytes = 0;
    stats.total_requests         = total_requests_.load();
    stats.total_rate_limited     = total_rate_limited_.load();

    for (const auto& [ip, record] : ip_records_) {
        stats.total_timestamps += record.timestamps.size();
    }

    stats.estimated_memory_bytes = estimate_memory_usage();

    return stats;
}

void RateLimiter::reset_stats() {
    std::lock_guard<std::mutex> lock(mutex_);

    total_requests_     = 0;
    total_rate_limited_ = 0;

    LOG_INFO("Rate limiter statistics reset");
}

}  // namespace ip_server