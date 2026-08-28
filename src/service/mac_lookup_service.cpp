#include "mac_lookup_service.h"

#include <chrono>
#include <stdexcept>

#include "logger.h"
#include "types.h"

namespace ip_server {

MACLookupService::MACLookupService(const std::string& oui_db_path, size_t cache_size)
    : cache_(cache_size, 8, 50 * 1024 * 1024) {  // MAC service: 50MB
    if (!oui_db_.open(oui_db_path)) {
        throw std::runtime_error("Failed to open OUI database: " + oui_db_path);
    }

    LOG_INFO("MACLookupService initialized with cache size: " + std::to_string(cache_size));
}

LookupResult MACLookupService::lookup(const std::string& mac_address) const {
    if (auto cached = cache_.get(mac_address)) {
        LOG_DEBUG("Cache hit for MAC: " + mac_address);
        return LookupResult(cached.value(), true);
    }

    nlohmann::json result;

    try {
        result = oui_db_.lookup(mac_address);

        // Don't cache errors as negatives
        if (!result.contains("error")) {
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

    return LookupResult(result, false);
}

}  // namespace ip_server
