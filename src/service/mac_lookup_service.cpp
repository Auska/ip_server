#include "mac_lookup_service.h"

#include <chrono>
#include <stdexcept>

#include "../logger.h"
#include "../types.h"

namespace ip_server {

MACLookupService::MACLookupService(const std::string& oui_db_path, size_t cache_size,
                                   size_t shard_count, size_t max_memory_bytes)
    : cache_(cache_size, shard_count, std::chrono::seconds(3600), max_memory_bytes) {
    if (!oui_db_.open(oui_db_path)) {
        throw std::runtime_error("Failed to open OUI database: " + oui_db_path);
    }

    LOG_INFO("MACLookupService initialized with cache size: " + std::to_string(cache_size)
             + ", shards: " + std::to_string(shard_count)
             + ", max memory: " + std::to_string(max_memory_bytes / static_cast<size_t>(1024 * 1024)) + "MB");
}

LookupResult MACLookupService::lookup(const std::string& mac_address) const {
    ScopedTimer timer;
    bool cache_hit = false;

    if (cache_enabled_) {
        auto cached = cache_.get(mac_address);
        if (cached.has_value()) {
            LOG_DEBUG("Cache hit for MAC: " + mac_address);
            return LookupResult(cached.value(), true, timer.elapsed());
        }
    }

    nlohmann::json result;

    try {
        result = oui_db_.lookup(mac_address);

        if (cache_enabled_) {
            if (result.value("found", false)) {
                cache_.put(mac_address, result, CacheDataType::MAC_OUI);
            } else {
                cache_.put(mac_address, result, CacheDataType::NEGATIVE);
            }
            LOG_DEBUG("Cached result for MAC: " + mac_address);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error during MAC lookup: " + std::string(e.what()));
        result["error"] = e.what();
    }

    return LookupResult(result, cache_hit, timer.elapsed());
}

}  // namespace ip_server
