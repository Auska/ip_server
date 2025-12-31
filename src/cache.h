#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <list>
#include <mutex>
#include <chrono>

namespace ip_server {

// Thread-safe LRU cache for IP lookup results
class IPCache {
public:
    explicit IPCache(size_t max_size = 1000, std::chrono::seconds ttl = std::chrono::seconds(3600))
        : max_size_(max_size), ttl_(ttl) {}

    // Get cached result if exists and not expired
    std::optional<nlohmann::json> get(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_map_.find(ip);
        if (it == cache_map_.end()) {
            return std::nullopt;
        }

        // Check if expired
        auto now = std::chrono::system_clock::now();
        if (now - it->second.timestamp > ttl_) {
            // Remove expired entry
            cache_list_.erase(it->second.list_it);
            cache_map_.erase(it);
            return std::nullopt;
        }

        // Move to front (most recently used)
        cache_list_.splice(cache_list_.begin(), cache_list_, it->second.list_it);

        return it->second.result;
    }

    // Put result into cache
    void put(const std::string& ip, const nlohmann::json& result) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_map_.find(ip);
        if (it != cache_map_.end()) {
            // Update existing entry
            it->second.result = result;
            it->second.timestamp = std::chrono::system_clock::now();
            cache_list_.splice(cache_list_.begin(), cache_list_, it->second.list_it);
            return;
        }

        // Add new entry
        cache_list_.push_front(ip);
        CacheEntry entry{result, std::chrono::system_clock::now(), cache_list_.begin()};
        cache_map_[ip] = entry;

        // Evict oldest if over capacity
        if (cache_map_.size() > max_size_) {
            auto oldest = cache_list_.back();
            cache_map_.erase(oldest);
            cache_list_.pop_back();
        }
    }

    // Clear cache
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_map_.clear();
        cache_list_.clear();
    }

    // Get cache statistics
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_map_.size();
    }

    void swap(IPCache& other) noexcept {
        std::lock(mutex_, other.mutex_);
        std::lock_guard<std::mutex> lock1(mutex_, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(other.mutex_, std::adopt_lock);
        cache_map_.swap(other.cache_map_);
        cache_list_.swap(other.cache_list_);
        std::swap(max_size_, other.max_size_);
        std::swap(ttl_, other.ttl_);
    }

    size_t max_size() const { return max_size_; }

private:
    struct CacheEntry {
        nlohmann::json result;
        std::chrono::system_clock::time_point timestamp;
        std::list<std::string>::iterator list_it;
    };

    std::unordered_map<std::string, CacheEntry> cache_map_;
    std::list<std::string> cache_list_;
    mutable std::mutex mutex_;
    size_t max_size_;
    std::chrono::seconds ttl_;
};

} // namespace ip_server