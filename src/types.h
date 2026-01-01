#pragma once

#include <nlohmann/json.hpp>

namespace ip_server {

// Result of an IP lookup with cache hit information
struct LookupResult {
    nlohmann::json data;
    bool cache_hit;
    double latency_ms;

    LookupResult() : cache_hit(false), latency_ms(0.0) {}
    LookupResult(const nlohmann::json& d, bool hit, double latency)
        : data(d), cache_hit(hit), latency_ms(latency) {}
};

} // namespace ip_server