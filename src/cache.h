#pragma once

#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <list>
#include <memory>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ip_server {

// Simple Bloom Filter implementation for cache penetration protection
template <size_t Bits = 65536, size_t HashCount = 3>
class BloomFilter {
   public:
    BloomFilter() = default;

    void add(const std::string& key) {
        for (size_t i = 0; i < HashCount; ++i) {
            size_t hash = hash_function(key, i);
            bits_[hash % Bits].test_and_set(std::memory_order_relaxed);
        }
    }

    bool possibly_contains(const std::string& key) const {
        for (size_t i = 0; i < HashCount; ++i) {
            size_t hash = hash_function(key, i);
            if (!bits_[hash % Bits].test(std::memory_order_relaxed)) {
                return false;
            }
        }
        return true;
    }

    void clear() {
        for (auto& bit : bits_) {
            bit.clear(std::memory_order_relaxed);
        }
    }

    double estimated_size() const {
        size_t set_bits = 0;
        for (const auto& bit : bits_) {
            if (bit.test(std::memory_order_relaxed)) {
                set_bits++;
            }
        }
        if (set_bits == 0) return 0.0;
        double ratio = static_cast<double>(set_bits) / Bits;
        return -Bits * std::log(1.0 - ratio) / HashCount;
    }

   private:
    size_t hash_function(const std::string& key, size_t seed) const {
        size_t hash = std::hash<std::string>{}(key);
        hash ^= seed * 0x9e3779b9;  // Golden ratio hash mixing
        hash ^= hash >> 16;
        hash *= 0x85ebca6b;
        hash ^= hash >> 13;
        hash *= 0xc2b2ae35;
        hash ^= hash >> 16;
        return hash;
    }

    std::array<std::atomic_flag, Bits> bits_{};
};

// Data type for differentiated TTL
enum class CacheDataType {
    IP_GEOLOCATION,  // IP geolocation data
    IP_ASN,          // IP ASN data
    MAC_OUI,         // MAC OUI data
    NEGATIVE         // Negative cache (not found)
};

// Cache statistics structure
struct CacheStats {
    uint64_t total_lookups{0};
    uint64_t hits{0};
    uint64_t misses{0};
    uint64_t evictions{0};
    uint64_t expired_entries{0};
    uint64_t concurrent_accesses{0};
    uint64_t memory_usage_bytes{0};
    uint64_t negative_hits{0};
    uint64_t negative_misses{0};
    double avg_entry_size{0.0};

    double hit_rate() const {
        return total_lookups > 0 ? (static_cast<double>(hits) / total_lookups) * 100.0 : 0.0;
    }

    double get_memory_usage_mb() const { return memory_usage_bytes / (1024.0 * 1024.0); }

    void reset() {
        total_lookups       = 0;
        hits                = 0;
        misses              = 0;
        evictions           = 0;
        expired_entries     = 0;
        concurrent_accesses = 0;
        memory_usage_bytes  = 0;
        negative_hits       = 0;
        negative_misses     = 0;
        avg_entry_size      = 0.0;
    }
};

// Single LRU cache shard with its own shared_mutex
class CacheShard {
   public:
    explicit CacheShard(size_t max_size                  = 100,
                        std::chrono::seconds default_ttl = std::chrono::seconds(3600),
                        size_t max_memory_bytes          = 10 * 1024 * 1024)  // 10MB default
        : max_size_(max_size), default_ttl_(default_ttl), max_memory_bytes_(max_memory_bytes) {}

    // Get cached result if exists and not expired
    std::optional<nlohmann::json> get(const std::string& key, CacheStats& stats) {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        stats.total_lookups++;
        stats.concurrent_accesses++;

        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            stats.misses++;
            return std::nullopt;
        }

        // Check if expired
        auto now = std::chrono::system_clock::now();
        auto ttl = get_ttl_for_type(it->second.data_type);
        if (now - it->second.timestamp > ttl) {
            lock.unlock();
            std::unique_lock<std::shared_mutex> write_lock(mutex_);
            // Double-check after acquiring write lock
            it = cache_map_.find(key);
            if (it != cache_map_.end()) {
                cache_list_.erase(it->second.list_it);
                negative_cache_.erase(key);
                memory_usage_bytes_ -= it->second.size_bytes;
                cache_map_.erase(it);
                stats.expired_entries++;
            }
            stats.misses++;
            return std::nullopt;
        }

        // Move to front (most recently used) - need write lock for this
        lock.unlock();
        std::unique_lock<std::shared_mutex> write_lock(mutex_);
        it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            cache_list_.splice(cache_list_.begin(), cache_list_, it->second.list_it);
            stats.hits++;
            if (it->second.data_type == CacheDataType::NEGATIVE) {
                stats.negative_hits++;
            }
            return it->second.result;
        }

        return std::nullopt;
    }

    // Put result into cache
    void put(std::string key, nlohmann::json result, CacheStats& stats,
             CacheDataType data_type = CacheDataType::IP_GEOLOCATION) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        size_t entry_size = result.dump().size();
        stats.avg_entry_size =
            (stats.avg_entry_size * (cache_map_.size()) + entry_size) / (cache_map_.size() + 1);

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Update existing entry
            memory_usage_bytes_ -= it->second.size_bytes;
            it->second.result     = std::move(result);
            it->second.timestamp  = std::chrono::system_clock::now();
            it->second.size_bytes = entry_size;
            it->second.data_type  = data_type;
            memory_usage_bytes_ += entry_size;
            cache_list_.splice(cache_list_.begin(), cache_list_, it->second.list_it);
            return;
        }

        // Check memory limit before adding
        evict_if_needed(entry_size, stats);

        // Add new entry
        cache_list_.push_front(key);
        CacheEntry entry{std::move(result), std::chrono::system_clock::now(), cache_list_.begin(),
                         entry_size, data_type};
        cache_map_.emplace(std::move(key), std::move(entry));
        memory_usage_bytes_ += entry_size;

        // Update negative cache if needed
        if (data_type == CacheDataType::NEGATIVE) {
            negative_cache_.insert(cache_list_.front());
        }

        // Evict oldest if over capacity
        if (cache_map_.size() > max_size_) {
            auto oldest = cache_list_.back();
            evict_entry(oldest, stats);
        }
    }

    // Clear cache
    void clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_map_.clear();
        cache_list_.clear();
        negative_cache_.clear();
        memory_usage_bytes_ = 0;
    }

    // Get shard size
    size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return cache_map_.size();
    }

    size_t memory_usage() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return memory_usage_bytes_;
    }

    size_t max_size() const { return max_size_; }
    size_t max_memory_bytes() const { return max_memory_bytes_; }

    void set_ttl(CacheDataType type, std::chrono::seconds ttl) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        ttl_config_[type] = ttl;
    }

   private:
    struct CacheEntry {
        nlohmann::json result;
        std::chrono::system_clock::time_point timestamp;
        std::list<std::string>::iterator list_it;
        size_t size_bytes;
        CacheDataType data_type;
    };

    std::chrono::seconds get_ttl_for_type(CacheDataType type) const {
        auto it = ttl_config_.find(type);
        if (it != ttl_config_.end()) {
            return it->second;
        }
        return default_ttl_;
    }

    void evict_entry(const std::string& key, CacheStats& stats) {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            memory_usage_bytes_ -= it->second.size_bytes;
            negative_cache_.erase(key);
            cache_list_.erase(it->second.list_it);
            cache_map_.erase(it);
            stats.evictions++;
        }
    }

    void evict_if_needed(size_t new_entry_size, CacheStats& stats) {
        // Evict based on memory limit
        while (!cache_list_.empty() && (memory_usage_bytes_ + new_entry_size > max_memory_bytes_)) {
            auto oldest = cache_list_.back();
            evict_entry(oldest, stats);
        }
    }

    std::unordered_map<std::string, CacheEntry> cache_map_;
    std::list<std::string> cache_list_;
    std::unordered_set<std::string> negative_cache_;
    std::unordered_map<CacheDataType, std::chrono::seconds> ttl_config_;
    mutable std::shared_mutex mutex_;
    size_t max_size_;
    std::chrono::seconds default_ttl_;
    size_t max_memory_bytes_;
    size_t memory_usage_bytes_{0};
};

// Per-shard statistics for monitoring
struct ShardStats {
    size_t shard_index;
    size_t size;
    size_t memory_usage_bytes;
    double get_memory_usage_mb() const { return memory_usage_bytes / (1024.0 * 1024.0); }
    uint64_t hits;
    uint64_t misses;
    double hit_rate;
};

// Heat map data for cache key distribution
struct CacheHeatMap {
    std::vector<std::pair<std::string, uint64_t>> hot_keys;  // Top N most accessed keys
    std::vector<size_t> shard_distribution;                  // Entries per shard
    uint64_t total_accesses;
};

// Thread-safe sharded LRU cache for IP lookup results
// Divides cache into multiple shards to reduce lock contention
class IPCache {
   public:
    explicit IPCache(size_t max_size = 10000, size_t shard_count = 8,
                     std::chrono::seconds default_ttl = std::chrono::seconds(3600),
                     size_t max_memory_bytes          = 100 * 1024 * 1024)  // 100MB default
        : shard_count_(shard_count), max_memory_bytes_(max_memory_bytes) {
        // Calculate shard size - for small caches, allow smaller per-shard sizes
        size_t shard_size   = (max_size + shard_count - 1) / shard_count;  // Ceiling division
        size_t shard_memory = (max_memory_bytes + shard_count - 1) / shard_count;
        shards_.reserve(shard_count);

        for (size_t i = 0; i < shard_count_; ++i) {
            shards_.push_back(std::make_unique<CacheShard>(shard_size, default_ttl, shard_memory));
        }

        // Configure default TTLs for different data types
        configure_default_ttls();
    }

    // Get cached result if exists and not expired
    std::optional<nlohmann::json> get(const std::string& key) {
        size_t shard_index = get_shard_index(key);
        track_key_access(key);
        return shards_[shard_index]->get(key, stats_);
    }

    // Put result into cache
    void put(std::string key, nlohmann::json result,
             CacheDataType data_type = CacheDataType::IP_GEOLOCATION) {
        size_t shard_index = get_shard_index(key);
        shards_[shard_index]->put(std::move(key), std::move(result), stats_, data_type);
    }

    // Clear cache
    void clear() {
        for (auto& shard : shards_) {
            shard->clear();
        }
        stats_.reset();
        key_access_counts_.clear();
    }

    // Get cache statistics
    CacheStats get_stats() const {
        CacheStats combined         = stats_;
        combined.memory_usage_bytes = get_total_memory_usage();
        return combined;
    }

    // Get per-shard statistics
    std::vector<ShardStats> get_shard_stats() const {
        std::vector<ShardStats> shard_stats;
        shard_stats.reserve(shard_count_);

        for (size_t i = 0; i < shard_count_; ++i) {
            ShardStats stats;
            stats.shard_index        = i;
            stats.size               = shards_[i]->size();
            stats.memory_usage_bytes = shards_[i]->memory_usage();
            // Note: Individual shard hit/miss tracking not implemented to avoid atomic overhead
            stats.hits     = 0;
            stats.misses   = 0;
            stats.hit_rate = 0.0;
            shard_stats.push_back(stats);
        }

        return shard_stats;
    }

    // Get heat map data
    CacheHeatMap get_heat_map(size_t top_n = 10) const {
        CacheHeatMap heat_map;
        heat_map.total_accesses = stats_.total_lookups;

        // Get top N hot keys
        std::vector<std::pair<std::string, uint64_t>> sorted_keys(key_access_counts_.begin(),
                                                                  key_access_counts_.end());
        std::partial_sort(sorted_keys.begin(),
                          sorted_keys.begin() + std::min(top_n, sorted_keys.size()),
                          sorted_keys.end(),
                          [](const auto& a, const auto& b) { return a.second > b.second; });

        heat_map.hot_keys.assign(sorted_keys.begin(),
                                 sorted_keys.begin() + std::min(top_n, sorted_keys.size()));

        // Get shard distribution
        heat_map.shard_distribution.reserve(shard_count_);
        for (const auto& shard : shards_) {
            heat_map.shard_distribution.push_back(shard->size());
        }

        return heat_map;
    }

    size_t size() const {
        size_t total = 0;
        for (const auto& shard : shards_) {
            total += shard->size();
        }
        return total;
    }

    size_t get_total_memory_usage() const {
        size_t total = 0;
        for (const auto& shard : shards_) {
            total += shard->memory_usage();
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

    size_t max_memory_bytes() const { return max_memory_bytes_; }

    void set_ttl(CacheDataType type, std::chrono::seconds ttl) {
        for (auto& shard : shards_) {
            shard->set_ttl(type, ttl);
        }
    }

   private:
    void configure_default_ttls() {
        // Set default TTLs for different data types
        for (auto& shard : shards_) {
            shard->set_ttl(CacheDataType::IP_GEOLOCATION, std::chrono::seconds(3600));  // 1 hour
            shard->set_ttl(CacheDataType::IP_ASN, std::chrono::seconds(86400));         // 24 hours
            shard->set_ttl(CacheDataType::MAC_OUI, std::chrono::seconds(86400 * 7));    // 7 days
            shard->set_ttl(CacheDataType::NEGATIVE, std::chrono::seconds(300));         // 5 minutes
        }
    }

    // Hash function to determine shard index
    size_t get_shard_index(const std::string& key) const {
        // Use std::hash for consistent distribution
        return std::hash<std::string>{}(key) % shard_count_;
    }

    // Track key access for heat map (thread-safe with relaxed ordering for performance)
    void track_key_access(const std::string& key) {
        // Use relaxed ordering since exact count is not critical
        key_access_counts_[key].fetch_add(1, std::memory_order_relaxed);
    }

    std::vector<std::unique_ptr<CacheShard>> shards_;
    size_t shard_count_;
    size_t max_memory_bytes_;
    mutable CacheStats stats_;
    mutable std::unordered_map<std::string, std::atomic<uint64_t>> key_access_counts_;
};

}  // namespace ip_server