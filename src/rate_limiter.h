#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <deque>
#include <atomic>

namespace ip_server {

class RateLimiter {
public:
    RateLimiter(int max_requests, std::chrono::seconds window, size_t max_ip_records = 10000)
        : max_requests_(max_requests),
          window_(window),
          max_ip_records_(max_ip_records),
          total_requests_(0),
          total_rate_limited_(0) {}

    // Check if request is allowed, returns true if allowed, false if rate limited
    bool is_allowed(const std::string& ip_address);

    // Get remaining requests for an IP
    int get_remaining(const std::string& ip_address) const;

    // Clean up old entries (call periodically)
    void cleanup();

    // Get memory statistics
    struct MemoryStats {
        size_t ip_record_count;
        size_t total_timestamps;
        size_t estimated_memory_bytes;
        uint64_t total_requests;
        uint64_t total_rate_limited;
    };
    MemoryStats get_memory_stats() const;

    // Reset statistics
    void reset_stats();

private:
    struct IPRecord {
        std::deque<std::chrono::steady_clock::time_point> timestamps;
        std::chrono::steady_clock::time_point last_access;
    };

    // Clean up old timestamps for a specific IP
    void cleanup_old_timestamps(IPRecord& record) const;

    // Estimate memory usage
    size_t estimate_memory_usage() const;

    int max_requests_;
    std::chrono::seconds window_;
    size_t max_ip_records_;
    std::unordered_map<std::string, IPRecord> ip_records_;
    mutable std::mutex mutex_;

    // Statistics
    std::atomic<uint64_t> total_requests_;
    std::atomic<uint64_t> total_rate_limited_;
};

} // namespace ip_server