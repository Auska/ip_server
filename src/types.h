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

    // Move constructor
    LookupResult(LookupResult&& other) noexcept
        : data(std::move(other.data)), cache_hit(other.cache_hit), latency_ms(other.latency_ms) {}

    // Move assignment
    LookupResult& operator=(LookupResult&& other) noexcept {
        if (this != &other) {
            data       = std::move(other.data);
            cache_hit  = other.cache_hit;
            latency_ms = other.latency_ms;
        }
        return *this;
    }

    // Disable copy to enforce move semantics
    LookupResult(const LookupResult&)            = delete;
    LookupResult& operator=(const LookupResult&) = delete;
};

}  // namespace ip_server