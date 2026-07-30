#pragma once

#include <chrono>
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

/// RAII timer that records elapsed time in milliseconds.
/// Call elapsed() for early-read, or let destructor write to output on scope exit.
class ScopedTimer {
   public:
    explicit ScopedTimer(double* out_ms = nullptr)
        : out_(out_ms), start_(std::chrono::high_resolution_clock::now()) {}

    ~ScopedTimer() {
        if (out_) *out_ = elapsed();
    }

    double elapsed() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

   private:
    double* out_;
    std::chrono::high_resolution_clock::time_point const start_;
};

}  // namespace ip_server
