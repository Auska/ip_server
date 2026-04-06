#pragma once

#include <chrono>
#include <string>

#include "../cache.h"
#include "../types.h"
#include "../mac_database.h"

namespace ip_server {

class MACLookupService {
   public:
    explicit MACLookupService(const std::string& oui_db_path, size_t cache_size = 10000,
                              size_t shard_count = 8, size_t max_memory_bytes = 50 * 1024 * 1024);
    ~MACLookupService() = default;

    MACLookupService(const MACLookupService&) = delete;
    MACLookupService& operator=(const MACLookupService&) = delete;

    LookupResult lookup(const std::string& mac_address) const;

    bool is_oui_db_open() const { return oui_db_.is_open(); }

    CacheStats get_cache_stats() const { return cache_.get_stats(); }
    std::vector<ShardStats> get_shard_stats() const { return cache_.get_shard_stats(); }
    CacheHeatMap get_heat_map(size_t top_n = 10) const { return cache_.get_heat_map(top_n); }
    size_t get_cache_size() const { return cache_.size(); }
    size_t get_cache_memory_usage() const { return cache_.get_total_memory_usage(); }
    void clear_cache() { cache_.clear(); }

    void set_cache_ttl(CacheDataType type, std::chrono::seconds ttl) { cache_.set_ttl(type, ttl); }

   private:
    OUIDatabase oui_db_;
    mutable IPCache cache_;
    bool cache_enabled_ = true;
};

}  // namespace ip_server
