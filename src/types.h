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

    // Move constructor
    LookupResult(LookupResult&& other) noexcept
        : data_(std::move(other.data_)), cache_hit_(other.cache_hit_), latency_ms_(other.latency_ms_) {}

    // Move assignment
    LookupResult& operator=(LookupResult&& other) noexcept {
        if (this != &other) {
            data_       = std::move(other.data_);
            cache_hit_  = other.cache_hit_;
            latency_ms_ = other.latency_ms_;
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
