#include "ip_geo_service.h"

#include "logger.h"
#include "types.h"

namespace ip_server {

IPGeoService::IPGeoService(const std::string& city_db_path, const std::string& asn_db_path)
    : city_db_(city_db_path), asn_db_(asn_db_path) {
    LOG_INFO("IPGeoService initialized");
}

LookupResult IPGeoService::lookup(const std::string& ip_address) const {
    if (auto cached = cache_.get(ip_address)) {
        LOG_DEBUG("Cache hit for IP: " + ip_address);
        return LookupResult(cached.value(), true);
    }

    nlohmann::json result;
    result["ip"]    = ip_address;
    result["found"] = false;

    try {
        auto city_result = city_db_.lookup(ip_address);
        auto asn_result  = asn_db_.lookup(ip_address);

        if (city_result.contains("error")) {
            result["error"] = city_result["error"];
            return LookupResult(result, false);
        }

        bool const city_found = city_result.value("found", false);
        bool const asn_found  = asn_result.value("found", false);

        if (!city_found && !asn_found) {
            cache_.put(ip_address, result, CacheDataType::NEGATIVE);
            LOG_DEBUG("Cached negative result for IP: " + ip_address);
            return LookupResult(result, false);
        }

        result["found"] = true;

        if (city_found) result.update(city_result);
        if (asn_found) result.update(asn_result);

        cache_.put(ip_address, result, CacheDataType::IP_GEOLOCATION);
        LOG_DEBUG("Cached result for IP: " + ip_address);

    } catch (const std::exception& e) {
        LOG_ERROR("Error during IP lookup: " + std::string(e.what()));
        result["error"] = e.what();
    }

    return LookupResult(result, false);
}

}  // namespace ip_server
