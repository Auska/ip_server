#include "ip_geo_service.h"

#include <chrono>
#include <stdexcept>

#include "../logger.h"

namespace ip_server {

IPGeoService::IPGeoService(const std::string& city_db_path, const std::string& asn_db_path,
                           size_t cache_size, size_t shard_count, size_t max_memory_bytes)
    : cache_(cache_size, shard_count, std::chrono::seconds(3600), max_memory_bytes) {
    if (!city_db_.open(city_db_path)) {
        throw std::runtime_error("Failed to open City database: " + city_db_path);
    }

    if (!asn_db_.open(asn_db_path)) {
        throw std::runtime_error("Failed to open ASN database: " + asn_db_path);
    }

    LOG_INFO("IPGeoService initialized with cache size: " + std::to_string(cache_size)
             + ", shards: " + std::to_string(shard_count)
             + ", max memory: " + std::to_string(max_memory_bytes / static_cast<size_t>(1024 * 1024)) + "MB");
}

LookupResult IPGeoService::lookup(const std::string& ip_address) const {
    auto start = std::chrono::high_resolution_clock::now();
    bool cache_hit = false;

    if (cache_enabled_) {
        auto cached = cache_.get(ip_address);
        if (cached.has_value()) {
            LOG_DEBUG("Cache hit for IP: " + ip_address);
            cache_hit = true;
            auto end = std::chrono::high_resolution_clock::now();
            double const latency_ms =
                std::chrono::duration<double, std::milli>(end - start).count();
            return LookupResult(cached.value(), true, latency_ms);
        }
    }

    nlohmann::json result;
    result["ip"] = ip_address;
    result["found"] = false;

    try {
        auto city_result = city_db_.lookup(ip_address);
        auto asn_result = asn_db_.lookup(ip_address);

        if (city_result.contains("error")) {
            result["error"] = city_result["error"];
            auto end = std::chrono::high_resolution_clock::now();
            double const latency_ms =
                std::chrono::duration<double, std::milli>(end - start).count();
            return LookupResult(result, false, latency_ms);
        }

        bool const city_found = city_result.value("found", false);
        bool const asn_found  = asn_result.value("found", false);

        if (!city_found && !asn_found) {
            if (cache_enabled_) {
                cache_.put(ip_address, result, CacheDataType::NEGATIVE);
                LOG_DEBUG("Cached negative result for IP: " + ip_address);
            }
            auto end = std::chrono::high_resolution_clock::now();
            double const latency_ms =
                std::chrono::duration<double, std::milli>(end - start).count();
            return LookupResult(result, false, latency_ms);
        }

        result["found"] = true;

        if (city_found) merge_city_result(result, city_result);
        if (asn_found) merge_asn_result(result, asn_result);

        if (cache_enabled_) {
            cache_.put(ip_address, result, CacheDataType::IP_GEOLOCATION);
            LOG_DEBUG("Cached result for IP: " + ip_address);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error during IP lookup: " + std::string(e.what()));
        result["error"] = e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double const latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
    return LookupResult(result, cache_hit, latency_ms);
}

void IPGeoService::merge_city_result(nlohmann::json& result, const nlohmann::json& city_result) {
    if (city_result.contains("country")) result["country"] = city_result["country"];
    if (city_result.contains("country_code")) result["country_code"] = city_result["country_code"];
    if (city_result.contains("city")) result["city"] = city_result["city"];
    if (city_result.contains("continent")) result["continent"] = city_result["continent"];
    if (city_result.contains("latitude")) result["latitude"] = city_result["latitude"];
    if (city_result.contains("longitude")) result["longitude"] = city_result["longitude"];
    if (city_result.contains("timezone")) result["timezone"] = city_result["timezone"];
}

void IPGeoService::merge_asn_result(nlohmann::json& result, const nlohmann::json& asn_result) {
    if (asn_result.contains("as_organization")) result["as_organization"] = asn_result["as_organization"];
    if (asn_result.contains("as_number")) result["as_number"] = asn_result["as_number"];
}

}  // namespace ip_server
