#pragma once

#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ip_server {

namespace cache_constants {
constexpr size_t DEFAULT_SHARD_MEMORY = 10 * 1024 * 1024;
constexpr size_t DEFAULT_MAX_MEMORY = 100 * 1024 * 1024;
}  // namespace cache_constants

enum class CacheDataType {
    IP_GEOLOCATION,
    MAC_OUI,
    NEGATIVE
};

struct CacheStats {
    uint64_t total_lookups_{0};
    uint64_t hits_{0};
    uint64_t misses_{0};
    uint64_t evictions_{0};
    uint64_t expired_entries_{0};
    uint64_t memory_usage_bytes_{0};

    double hit_rate() const {
        return total_lookups_ > 0 ? (static_cast<double>(hits_) / total_lookups_) * 100.0 : 0.0;
    }

    double get_memory_usage_mb() const { return memory_usage_bytes_ / (1024.0 * 1024.0); }

    CacheStats& operator+=(const CacheStats& o) {
        total_lookups_ += o.total_lookups_;
        hits_ += o.hits_;
        misses_ += o.misses_;
        evictions_ += o.evictions_;
        expired_entries_ += o.expired_entries_;
        memory_usage_bytes_ += o.memory_usage_bytes_;
        return *this;
    }
};

class CacheShard {
   public:
    explicit CacheShard(size_t max_size = 100,
                        size_t max_memory_bytes = cache_constants::DEFAULT_SHARD_MEMORY)
        : max_size_(max_size), max_memory_bytes_(max_memory_bytes) {}

    std::optional<nlohmann::json> get(const std::string& key) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        stats_.total_lookups_++;

        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            stats_.misses_++;
            return std::nullopt;
        }

        auto now = std::chrono::system_clock::now();
        auto ttl = get_ttl_for_type(it->second.data_type_);
        if (now - it->second.timestamp_ > ttl) {
            size_t entry_size = it->second.size_bytes_;
            cache_list_.erase(it->second.list_it_);
            memory_usage_bytes_ -= entry_size;
            cache_map_.erase(it);
            stats_.expired_entries_++;
            stats_.misses_++;
            return std::nullopt;
        }

        cache_list_.splice(cache_list_.begin(), cache_list_, it->second.list_it_);
        stats_.hits_++;
        return it->second.result_;
    }

    void put(std::string key, nlohmann::json result,
             CacheDataType data_type = CacheDataType::IP_GEOLOCATION) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        size_t entry_size = estimate_entry_size(key, result);

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            memory_usage_bytes_ -= it->second.size_bytes_;
            it->second.result_ = std::move(result);
            it->second.timestamp_ = std::chrono::system_clock::now();
            it->second.size_bytes_ = entry_size;
            it->second.data_type_ = data_type;
            memory_usage_bytes_ += entry_size;
            cache_list_.splice(cache_list_.begin(), cache_list_, it->second.list_it_);
            return;
        }

        evict_if_needed(entry_size, stats_);

        cache_list_.push_front(key);
        CacheEntry entry{std::move(result), std::chrono::system_clock::now(), cache_list_.begin(),
                         entry_size, data_type};
        cache_map_.emplace(std::move(key), std::move(entry));
        memory_usage_bytes_ += entry_size;

        if (cache_map_.size() > max_size_) {
            auto oldest = cache_list_.back();
            evict_entry(oldest, stats_);
        }
    }

    void clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_map_.clear();
        cache_list_.clear();
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

    void set_ttl(CacheDataType type, std::chrono::seconds ttl) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        ttl_config_[type] = ttl;
    }

   private:
    struct CacheEntry {
        nlohmann::json result_;
        std::chrono::system_clock::time_point timestamp_;
        std::list<std::string>::iterator list_it_;
        size_t size_bytes_;
        CacheDataType data_type_;
    };

    size_t estimate_entry_size(const std::string& key, const nlohmann::json& result) const {
        return key.size() * 2 + result.dump().size();
    }

    std::chrono::seconds get_ttl_for_type(CacheDataType type) const {
        auto it = ttl_config_.find(type);
        if (it != ttl_config_.end()) {
            return it->second;
        }
        return std::chrono::seconds(3600);
    }

    void evict_entry(const std::string& key, CacheStats& stats) {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            memory_usage_bytes_ -= it->second.size_bytes_;
            cache_list_.erase(it->second.list_it_);
            cache_map_.erase(it);
            stats.evictions_++;
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
    std::unordered_map<CacheDataType, std::chrono::seconds> ttl_config_;
    mutable std::shared_mutex mutex_;
    size_t max_size_;
    size_t max_memory_bytes_;
    size_t memory_usage_bytes_{0};
    CacheStats stats_;
};

class IPCache {
   public:
    explicit IPCache(size_t max_size = 10000, size_t shard_count = 8,
                     size_t max_memory_bytes = cache_constants::DEFAULT_MAX_MEMORY)
        : shard_count_(shard_count) {
        size_t shard_size = (max_size + shard_count - 1) / shard_count;
        size_t shard_memory = (max_memory_bytes + shard_count - 1) / shard_count;
        shards_.reserve(shard_count);

        for (size_t i = 0; i < shard_count_; ++i) {
            shards_.push_back(std::make_unique<CacheShard>(shard_size, shard_memory));
        }

        configure_default_ttls();
    }

    std::optional<nlohmann::json> get(const std::string& key) {
        size_t shard_index = get_shard_index(key);
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
    }

    CacheStats get_stats() const {
        CacheStats combined;
        for (const auto& shard : shards_) {
            combined += shard->get_local_stats();
        }
        combined.memory_usage_bytes_ = get_total_memory_usage();
        return combined;
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

    void set_ttl(CacheDataType type, std::chrono::seconds ttl) {
        for (auto& shard : shards_) {
            shard->set_ttl(type, ttl);
        }
    }

   private:
    void configure_default_ttls() {
        for (auto& shard : shards_) {
            shard->set_ttl(CacheDataType::IP_GEOLOCATION, std::chrono::seconds(3600));
            shard->set_ttl(CacheDataType::MAC_OUI, std::chrono::seconds(86400 * 7));
            shard->set_ttl(CacheDataType::NEGATIVE, std::chrono::seconds(300));
        }
    }

    size_t get_shard_index(const std::string& key) const {
        return std::hash<std::string>{}(key) % shard_count_;
    }

    std::vector<std::unique_ptr<CacheShard>> shards_;
    size_t shard_count_;
};

}  // namespace ip_server
