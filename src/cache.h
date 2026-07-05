#pragma once

#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ip_server {

namespace cache_constants {
constexpr size_t DEFAULT_SHARD_MEMORY = 10 * 1024 * 1024;
constexpr size_t DEFAULT_MAX_MEMORY = 100 * 1024 * 1024;
constexpr size_t DEFAULT_BLOOM_BITS = 65536;
constexpr size_t DEFAULT_BLOOM_HASHES = 3;
constexpr size_t ESTIMATED_JSON_OVERHEAD = 200;
}  // namespace cache_constants

template <size_t Bits = cache_constants::DEFAULT_BLOOM_BITS,
          size_t HashCount = cache_constants::DEFAULT_BLOOM_HASHES>
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
        hash ^= seed * 0x9e3779b9;
        hash ^= hash >> 16;
        hash *= 0x85ebca6b;
        hash ^= hash >> 13;
        hash *= 0xc2b2ae35;
        hash ^= hash >> 16;
        return hash;
    }

    std::array<std::atomic_flag, Bits> bits_{};
};

enum class CacheDataType {
    IP_GEOLOCATION,
    IP_ASN,
    MAC_OUI,
    NEGATIVE
};

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
        total_lookups = 0;
        hits = 0;
        misses = 0;
        evictions = 0;
        expired_entries = 0;
        concurrent_accesses = 0;
        memory_usage_bytes = 0;
        negative_hits = 0;
        negative_misses = 0;
        avg_entry_size = 0.0;
    }

    CacheStats& operator+=(const CacheStats& o) {
        total_lookups += o.total_lookups;
        hits += o.hits;
        misses += o.misses;
        evictions += o.evictions;
        expired_entries += o.expired_entries;
        concurrent_accesses += o.concurrent_accesses;
        memory_usage_bytes += o.memory_usage_bytes;
        negative_hits += o.negative_hits;
        negative_misses += o.negative_misses;
        avg_entry_size = (avg_entry_size + o.avg_entry_size) / 2.0;
        return *this;
    }
};

class CacheShard {
   public:
    explicit CacheShard(size_t max_size = 100,
                        std::chrono::seconds default_ttl = std::chrono::seconds(3600),
                        size_t max_memory_bytes = cache_constants::DEFAULT_SHARD_MEMORY)
        : max_size_(max_size), default_ttl_(default_ttl), max_memory_bytes_(max_memory_bytes) {}

    std::optional<nlohmann::json> get(const std::string& key) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        stats_.total_lookups++;
        stats_.concurrent_accesses++;

        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            stats_.misses++;
            return std::nullopt;
        }

        auto now = std::chrono::system_clock::now();
        auto ttl = get_ttl_for_type(it->second.data_type);
        if (now - it->second.timestamp > ttl) {
            size_t entry_size = it->second.size_bytes;
            cache_list_.erase(it->second.list_it);
            negative_cache_.erase(key);
            memory_usage_bytes_ -= entry_size;
            cache_map_.erase(it);
            stats_.expired_entries++;
            stats_.misses++;
            return std::nullopt;
        }

        cache_list_.splice(cache_list_.begin(), cache_list_, it->second.list_it);
        stats_.hits++;
        if (it->second.data_type == CacheDataType::NEGATIVE) {
            stats_.negative_hits++;
        }
        return it->second.result;
    }

    void put(std::string key, nlohmann::json result,
             CacheDataType data_type = CacheDataType::IP_GEOLOCATION) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        size_t entry_size = estimate_entry_size(key, result);
        stats_.avg_entry_size =
            (stats_.avg_entry_size * (cache_map_.size()) + entry_size) / (cache_map_.size() + 1);

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            memory_usage_bytes_ -= it->second.size_bytes;
            it->second.result = std::move(result);
            it->second.timestamp = std::chrono::system_clock::now();
            it->second.size_bytes = entry_size;
            it->second.data_type = data_type;
            memory_usage_bytes_ += entry_size;
            cache_list_.splice(cache_list_.begin(), cache_list_, it->second.list_it);
            return;
        }

        evict_if_needed(entry_size, stats_);

        cache_list_.push_front(key);
        CacheEntry entry{std::move(result), std::chrono::system_clock::now(), cache_list_.begin(),
                         entry_size, data_type};
        cache_map_.emplace(std::move(key), std::move(entry));
        memory_usage_bytes_ += entry_size;

        if (data_type == CacheDataType::NEGATIVE) {
            negative_cache_.insert(cache_list_.front());
        }

        if (cache_map_.size() > max_size_) {
            auto oldest = cache_list_.back();
            evict_entry(oldest, stats_);
        }
    }

    void clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_map_.clear();
        cache_list_.clear();
        negative_cache_.clear();
        memory_usage_bytes_ = 0;
    }

    size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return cache_map_.size();
    }

    size_t memory_usage() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return memory_usage_bytes_;
    }

    CacheStats get_local_stats() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return stats_;
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

    size_t estimate_entry_size(const std::string& key, const nlohmann::json& result) const {
        size_t json_size = cache_constants::ESTIMATED_JSON_OVERHEAD;
        if (result.is_object()) {
            for (auto it = result.begin(); it != result.end(); ++it) {
                json_size += it.key().size() + 4;
                if (it.value().is_string()) {
                    json_size += it.value().get<std::string>().size() + 2;
                }
            }
        } else if (result.is_string()) {
            json_size += result.get<std::string>().size();
        }
        return key.size() * 2 + json_size;
    }

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
    CacheStats stats_;
};

struct ShardStats {
    size_t shard_index;
    size_t size;
    size_t memory_usage_bytes;
    double get_memory_usage_mb() const { return memory_usage_bytes / (1024.0 * 1024.0); }
    uint64_t hits;
    uint64_t misses;
    double hit_rate;
};

struct CacheHeatMap {
    std::vector<std::pair<std::string, uint64_t>> hot_keys;
    std::vector<size_t> shard_distribution;
    uint64_t total_accesses;
};

class IPCache {
   public:
    explicit IPCache(size_t max_size = 10000, size_t shard_count = 8,
                     std::chrono::seconds default_ttl = std::chrono::seconds(3600),
                     size_t max_memory_bytes = cache_constants::DEFAULT_MAX_MEMORY)
        : shard_count_(shard_count), max_memory_bytes_(max_memory_bytes) {
        size_t shard_size = (max_size + shard_count - 1) / shard_count;
        size_t shard_memory = (max_memory_bytes + shard_count - 1) / shard_count;
        shards_.reserve(shard_count);

        for (size_t i = 0; i < shard_count_; ++i) {
            shards_.push_back(std::make_unique<CacheShard>(shard_size, default_ttl, shard_memory));
        }

        configure_default_ttls();
    }

    std::optional<nlohmann::json> get(const std::string& key) {
        size_t shard_index = get_shard_index(key);
        track_key_access(key);
        return shards_[shard_index]->get(key);
    }

    void put(std::string key, nlohmann::json result,
             CacheDataType data_type = CacheDataType::IP_GEOLOCATION) {
        size_t shard_index = get_shard_index(key);
        shards_[shard_index]->put(std::move(key), std::move(result), data_type);
    }

    void clear() {
        for (auto& shard : shards_) {
            shard->clear();
        }
        key_access_counts_.clear();
        key_access_lru_.clear();
        key_access_lru_it_.clear();
    }

    CacheStats get_stats() const {
        CacheStats combined;
        for (const auto& shard : shards_) {
            combined += shard->get_local_stats();
        }
        combined.memory_usage_bytes = get_total_memory_usage();
        return combined;
    }

    std::vector<ShardStats> get_shard_stats() const {
        std::vector<ShardStats> shard_stats;
        shard_stats.reserve(shard_count_);

        for (size_t i = 0; i < shard_count_; ++i) {
            ShardStats stats;
            stats.shard_index = i;
            stats.size = shards_[i]->size();
            stats.memory_usage_bytes = shards_[i]->memory_usage();
            stats.hits = 0;
            stats.misses = 0;
            stats.hit_rate = 0.0;
            shard_stats.push_back(stats);
        }

        return shard_stats;
    }

    CacheHeatMap get_heat_map(size_t top_n = 10) const {
        CacheHeatMap heat_map;
        heat_map.total_accesses = get_stats().total_lookups;

        std::vector<std::pair<std::string, uint64_t>> sorted_keys;
        sorted_keys.reserve(key_access_counts_.size());
        for (const auto& [key, count_ptr] : key_access_counts_) {
            sorted_keys.emplace_back(key, count_ptr->load(std::memory_order_relaxed));
        }
        std::partial_sort(sorted_keys.begin(),
                          sorted_keys.begin() + std::min(top_n, sorted_keys.size()),
                          sorted_keys.end(),
                          [](const auto& a, const auto& b) { return a.second > b.second; });

        heat_map.hot_keys.assign(sorted_keys.begin(),
                                 sorted_keys.begin() + std::min(top_n, sorted_keys.size()));

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
        for (auto& shard : shards_) {
            shard->set_ttl(CacheDataType::IP_GEOLOCATION, std::chrono::seconds(3600));
            shard->set_ttl(CacheDataType::IP_ASN, std::chrono::seconds(86400));
            shard->set_ttl(CacheDataType::MAC_OUI, std::chrono::seconds(86400 * 7));
            shard->set_ttl(CacheDataType::NEGATIVE, std::chrono::seconds(300));
        }
    }

    size_t get_shard_index(const std::string& key) const {
        return std::hash<std::string>{}(key) % shard_count_;
    }

    void track_key_access(const std::string& key) {
        auto it = key_access_counts_.find(key);
        if (it == key_access_counts_.end()) {
            if (key_access_counts_.size() >= MAX_TRACKED_KEYS) {
                auto oldest = key_access_lru_.back();
                key_access_counts_.erase(oldest);
                key_access_lru_.pop_back();
            }
            key_access_lru_.push_front(key);
            key_access_counts_.emplace(key, std::make_unique<std::atomic<uint64_t>>(1));
        } else {
            it->second->fetch_add(1, std::memory_order_relaxed);
            auto lru_it = key_access_lru_it_.find(key);
            if (lru_it != key_access_lru_it_.end()) {
                key_access_lru_.splice(key_access_lru_.begin(), key_access_lru_, lru_it->second);
            }
        }
    }

    std::vector<std::unique_ptr<CacheShard>> shards_;
    size_t shard_count_;
    size_t max_memory_bytes_;
    static constexpr size_t MAX_TRACKED_KEYS = 100000;
    mutable std::list<std::string> key_access_lru_;
    mutable std::unordered_map<std::string, std::list<std::string>::iterator> key_access_lru_it_;
    mutable std::unordered_map<std::string, std::unique_ptr<std::atomic<uint64_t>>> key_access_counts_;
};

}  // namespace ip_server
