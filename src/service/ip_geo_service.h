#pragma once

#include <string>

#include "cache.h"
#include "types.h"
#include "database/city_database.h"
#include "database/asn_database.h"

namespace ip_server {

class IPGeoService {
   public:
    explicit IPGeoService(const std::string& city_db_path, const std::string& asn_db_path);
    ~IPGeoService() = default;

    IPGeoService(const IPGeoService&) = delete;
    IPGeoService& operator=(const IPGeoService&) = delete;

    LookupResult lookup(const std::string& ip_address) const;

    CacheStats cache_stats() const { return cache_.get_stats(); }

   private:
    static void merge_city_result(nlohmann::json& result, const nlohmann::json& city_result);
    static void merge_asn_result(nlohmann::json& result, const nlohmann::json& asn_result);

    CityDatabase city_db_;
    ASNDatabase asn_db_;
    mutable IPCache cache_;
};

}  // namespace ip_server
