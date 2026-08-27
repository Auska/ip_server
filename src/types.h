#pragma once

#include <chrono>
#include <nlohmann/json.hpp>

namespace ip_server {

// Result of an IP lookup with cache hit information
struct LookupResult {
    nlohmann::json data_;
    bool cache_hit_;
    double latency_ms_;

    LookupResult() : cache_hit_(false), latency_ms_(0.0) {}
    LookupResult(const nlohmann::json& d, bool hit, double latency)
        : data_(d), cache_hit_(hit), latency_ms_(latency) {}

    LookupResult(LookupResult&&) noexcept            = default;
    LookupResult& operator=(LookupResult&&) noexcept = default;

    // Disable copy to enforce move semantics
    LookupResult(const LookupResult&)            = delete;
    LookupResult& operator=(const LookupResult&) = delete;
};

/// RAII timer that records elapsed time in milliseconds; read with elapsed().
class ScopedTimer {
   public:
    ScopedTimer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

   private:
    std::chrono::high_resolution_clock::time_point const start_;
};

}  // namespace ip_server
