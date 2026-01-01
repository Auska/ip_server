#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <deque>

namespace ip_server {

class RateLimiter {
public:
    RateLimiter(int max_requests, std::chrono::seconds window)
        : max_requests_(max_requests), window_(window) {}

    // Check if request is allowed, returns true if allowed, false if rate limited
    bool is_allowed(const std::string& ip_address);

    // Get remaining requests for an IP
    int get_remaining(const std::string& ip_address) const;

    // Clean up old entries (call periodically)
    void cleanup();

private:
    struct IPRecord {
        std::deque<std::chrono::steady_clock::time_point> timestamps;
    };

    int max_requests_;
    std::chrono::seconds window_;
    std::unordered_map<std::string, IPRecord> ip_records_;
    mutable std::mutex mutex_;
};

} // namespace ip_server