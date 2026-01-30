#pragma once

#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace ip_server {

// Cache statistics structure
struct CacheStats {
    uint64_t total_lookups{0};
    uint64_t hits{0};
    uint64_t misses{0};
    uint64_t evictions{0};
    uint64_t expired_entries{0};
    uint64_t concurrent_accesses{0};
    double avg_entry_size{0.0};

    double hit_rate() const {
        return total_lookups > 0 ? (static_cast<double>(hits) / total_lookups) * 100.0 : 0.0;
    }

    void reset() {
        total_lookups = 0;
        hits = 0;
        misses = 0;
        evictions = 0;
        expired_entries = 0;
        concurrent_accesses = 0;
        avg_entry_size = 0.0;
    }
};

// Single LRU cache shard with its own mutex
class CacheShard {
   public:
    explicit CacheShard(size_t max_size = 100, std::chrono::seconds ttl = std::chrono::seconds(3600))
        : max_size_(max_size), ttl_(ttl) {}

    // Get cached result if exists and not expired
    std::optional<nlohmann::json> get(const std::string& key, CacheStats& stats) {
        std::lock_guard<std::mutex> lock(mutex_);

        stats.total_lookups++;
        stats.concurrent_accesses++;

        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            stats.misses++;
            return std::nullopt;
        }

        // Check if expired
        auto now = std::chrono::system_clock::now();
        if (now - it->second.timestamp > ttl_) {
            // Remove expired entry
            cache_list_.erase(it->second.list_it);
            cache_map_.erase(it);
            stats.expired_entries++;
            stats.misses++;
            return std::nullopt;
        }

        // Move to front (most recently used)
        cache_list_.splice(cache_list_.begin(), cache_list_, it->second.list_it);
        stats.hits++;

        return it->second.result;
    }

    // Put result into cache
    void put(std::string key, nlohmann::json result, CacheStats& stats) {
        std::lock_guard<std::mutex> lock(mutex_);

        size_t entry_size = result.dump().size();
        stats.avg_entry_size = (stats.avg_entry_size * (cache_map_.size()) + entry_size)
                               / (cache_map_.size() + 1);

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Update existing entry
            it->second.result = std::move(result);
            it->second.timestamp = std::chrono::system_clock::now();
            cache_list_.splice(cache_list_.begin(), cache_list_, it->second.list_it);
            return;
        }

        // Add new entry
        cache_list_.push_front(key);
        CacheEntry entry{std::move(result), std::chrono::system_clock::now(), cache_list_.begin()};
        cache_map_.emplace(std::move(key), std::move(entry));

        // Evict oldest if over capacity
        if (cache_map_.size() > max_size_) {
            auto oldest = cache_list_.back();
            cache_map_.erase(oldest);
            cache_list_.pop_back();
            stats.evictions++;
        }
    }

    // Clear cache
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_map_.clear();
        cache_list_.clear();
    }

    // Get shard size
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_map_.size();
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

// Thread-safe sharded LRU cache for IP lookup results
// Divides cache into multiple shards to reduce lock contention
class IPCache {
   public:
    explicit IPCache(size_t max_size = 10000, size_t shard_count = 8,
                     std::chrono::seconds ttl = std::chrono::seconds(3600))
        : shard_count_(shard_count) {
        // Calculate shard size - for small caches, allow smaller per-shard sizes
        size_t shard_size = (max_size + shard_count - 1) / shard_count;  // Ceiling division
        shards_.reserve(shard_count);

        for (size_t i = 0; i < shard_count_; ++i) {
            shards_.push_back(std::make_unique<CacheShard>(shard_size, ttl));
        }
    }

    // Get cached result if exists and not expired
    std::optional<nlohmann::json> get(const std::string& key) {
        size_t shard_index = get_shard_index(key);
        return shards_[shard_index]->get(key, stats_);
    }

    // Put result into cache
    void put(std::string key, nlohmann::json result) {
        size_t shard_index = get_shard_index(key);
        shards_[shard_index]->put(std::move(key), std::move(result), stats_);
    }

    // Clear cache
    void clear() {
        for (auto& shard : shards_) {
            shard->clear();
        }
        stats_.reset();
    }

    // Get cache statistics
    CacheStats get_stats() const { return stats_; }

    size_t size() const {
        size_t total = 0;
        for (const auto& shard : shards_) {
            total += shard->size();
        }
        return total;
    }

    size_t shard_count() const { return shard_count_; }

    size_t max_size() const {
        size_t total = 0;
        for (const auto& shard : shards_) {
            total += shard->max_size();
        }
        return total;
    }

   private:
    // Hash function to determine shard index
    size_t get_shard_index(const std::string& key) const {
        // Use std::hash for consistent distribution
        return std::hash<std::string>{}(key) % shard_count_;
    }

    std::vector<std::unique_ptr<CacheShard>> shards_;
    size_t shard_count_;
    mutable CacheStats stats_;
};

}  // namespace ip_server