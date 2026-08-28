#include "rate_limiter.h"

#include <algorithm>

#include "logger.h"

namespace ip_server {

void RateLimiter::cleanup_old_timestamps(IPRecord& record) const {
    auto now = std::chrono::steady_clock::now();
    while (!record.timestamps_.empty() && now - record.timestamps_.front() > window_) {
        record.timestamps_.pop_front();
    }
}

void RateLimiter::evict_lru() {
    if (lru_list_.empty()) {
        return;
    }

    const std::string& lru_ip = lru_list_.back();
    LOG_DEBUG("Rate limiter: Evicting IP record (LRU): " + lru_ip);
    ip_records_.erase(lru_ip);
    lru_list_.pop_back();
}

bool RateLimiter::is_allowed(const std::string& ip_address) {
    std::scoped_lock const lock(mutex_);

    if (max_requests_ <= 0) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto it  = ip_records_.find(ip_address);

    if (it == ip_records_.end()) {
        if (ip_records_.size() >= max_ip_records_) {
            evict_lru();
        }

        lru_list_.push_front(ip_address);
        IPRecord record;
        record.timestamps_.push_back(now);
        record.lru_it_ = lru_list_.begin();
        ip_records_.emplace(ip_address, std::move(record));
        return true;
    }

    IPRecord& record = it->second;

    lru_list_.splice(lru_list_.begin(), lru_list_, record.lru_it_);
    cleanup_old_timestamps(record);

    if (record.timestamps_.size() < static_cast<size_t>(max_requests_)) {
        record.timestamps_.push_back(now);
        return true;
    }

    return false;
}

int RateLimiter::get_remaining(const std::string& ip_address) {
    std::scoped_lock const lock(mutex_);

    auto it = ip_records_.find(ip_address);
    if (it == ip_records_.end()) {
        return max_requests_;
    }

    cleanup_old_timestamps(it->second);

    return std::max(0, max_requests_ - static_cast<int>(it->second.timestamps_.size()));
}

void RateLimiter::cleanup() {
    std::scoped_lock const lock(mutex_);

    for (auto& [ip, record] : ip_records_) {
        cleanup_old_timestamps(record);
    }

    auto it              = ip_records_.begin();
    size_t removed_count = 0;
    while (it != ip_records_.end()) {
        if (it->second.timestamps_.empty()) {
            lru_list_.erase(it->second.lru_it_);
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

}  // namespace ip_server
