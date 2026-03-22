#pragma once

#include <maxminddb.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

#include "cache.h"
#include "mac_database.h"
#include "types.h"

namespace ip_server {

class MaxMindDatabase {
   public:
    MaxMindDatabase() = default;
    virtual ~MaxMindDatabase();

    MaxMindDatabase(const MaxMindDatabase&) = delete;
    MaxMindDatabase& operator=(const MaxMindDatabase&) = delete;
    MaxMindDatabase(MaxMindDatabase&&) noexcept;
    MaxMindDatabase& operator=(MaxMindDatabase&&) noexcept;

    bool open(const std::string& db_path);
    void close();
    bool is_open() const { return is_open_.load(std::memory_order_acquire); }

    nlohmann::json lookup(const std::string& ip_address) const;

   protected:
    MMDB_s mmdb_{};
    std::atomic<bool> is_open_{false};
    mutable std::mutex open_close_mutex_;

    nlohmann::json perform_lookup(const std::string& ip_address, int& gai_error, int& mmdb_error,
                                  MMDB_lookup_result_s& result) const;
};

class CityDatabase : public MaxMindDatabase {
   public:
    nlohmann::json lookup(const std::string& ip_address) const;
};

class ASNDatabase : public MaxMindDatabase {
   public:
    nlohmann::json lookup(const std::string& ip_address) const;
};

class IPGeoService {
   public:
    explicit IPGeoService(const std::string& city_db_path, const std::string& asn_db_path,
                          size_t cache_size = 10000, size_t shard_count = 8,
                          size_t max_memory_bytes = 100 * 1024 * 1024);
    ~IPGeoService() = default;

    IPGeoService(const IPGeoService&) = delete;
    IPGeoService& operator=(const IPGeoService&) = delete;

    LookupResult lookup(const std::string& ip_address) const;

    bool is_city_db_open() const { return city_db_.is_open(); }
    bool is_asn_db_open() const { return asn_db_.is_open(); }

    CacheStats get_cache_stats() const { return cache_.get_stats(); }
    std::vector<ShardStats> get_shard_stats() const { return cache_.get_shard_stats(); }
    CacheHeatMap get_heat_map(size_t top_n = 10) const { return cache_.get_heat_map(top_n); }
    size_t get_cache_size() const { return cache_.size(); }
    size_t get_cache_memory_usage() const { return cache_.get_total_memory_usage(); }
    void clear_cache() { cache_.clear(); }

    void set_cache_ttl(CacheDataType type, std::chrono::seconds ttl) { cache_.set_ttl(type, ttl); }

   private:
    CityDatabase city_db_;
    ASNDatabase asn_db_;
    mutable IPCache cache_;
    bool cache_enabled_ = true;
};

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
