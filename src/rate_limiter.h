#pragma once

#include <chrono>
#include <deque>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ip_server {

class RateLimiter {
   public:
    RateLimiter(int max_requests, std::chrono::seconds window, size_t max_ip_records = 10000)
        : max_requests_(max_requests),
          window_(window),
          max_ip_records_(max_ip_records) {}

    bool is_allowed(const std::string& ip_address);

    int get_remaining(const std::string& ip_address);

    void cleanup();

   private:
    struct IPRecord {
        std::deque<std::chrono::steady_clock::time_point> timestamps_;
        std::list<std::string>::iterator lru_it_;
    };

    void cleanup_old_timestamps(IPRecord& record) const;

    void evict_lru();

    int max_requests_;
    std::chrono::seconds window_;
    size_t max_ip_records_;

    std::unordered_map<std::string, IPRecord> ip_records_;
    std::list<std::string> lru_list_;
    mutable std::mutex mutex_;
};

}  // namespace ip_server
