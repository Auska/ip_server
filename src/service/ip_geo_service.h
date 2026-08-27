#pragma once

#include <chrono>
#include <string>

#include "cache.h"
#include "types.h"
#include "database/city_database.h"
#include "database/asn_database.h"

namespace ip_server {

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
    size_t get_cache_size() const { return cache_.size(); }
    size_t get_cache_memory_usage() const { return cache_.get_total_memory_usage(); }
    void clear_cache() { cache_.clear(); }

    void set_cache_ttl(CacheDataType type, std::chrono::seconds ttl) { cache_.set_ttl(type, ttl); }

   private:
    static void merge_city_result(nlohmann::json& result, const nlohmann::json& city_result);
    static void merge_asn_result(nlohmann::json& result, const nlohmann::json& asn_result);

    CityDatabase city_db_;
    ASNDatabase asn_db_;
    mutable IPCache cache_;
};

}  // namespace ip_server
