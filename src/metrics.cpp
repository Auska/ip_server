#include "metrics.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "logger.h"

namespace ip_server {

namespace {
constexpr size_t MAX_LATENCIES = 1000;
}

Metrics::Metrics()
    : total_requests_(0),
      cache_hits_(0),
      cache_misses_(0),
      cache_evictions_(0),
      total_errors_(0),
      start_time_(std::chrono::steady_clock::now()),
      city_db_open_(false),
      asn_db_open_(false),
      oui_db_open_(false) {
    LOG_INFO("Metrics collector initialized");
}

void Metrics::record_request(bool cache_hit, double latency_ms) {
    total_requests_++;
    if (cache_hit) {
        cache_hits_++;
    } else {
        cache_misses_++;
    }

    std::scoped_lock const lock(mutex_);
    latencies_.push_back(latency_ms);
    if (latencies_.size() > MAX_LATENCIES) {
        latencies_.pop_front();
    }

    auto now = std::chrono::steady_clock::now();
    request_timestamps_.emplace_back(now, total_requests_.load());

    auto cutoff = now - std::chrono::seconds(60);
    while (!request_timestamps_.empty() && request_timestamps_.front().first < cutoff) {
        request_timestamps_.pop_front();
    }
}

void Metrics::record_error() {
    total_errors_++;
}

void Metrics::record_cache_eviction() {
    cache_evictions_++;
}

Metrics::Stats Metrics::get_stats() const {
    Stats stats{};

    stats.total_requests = total_requests_.load();
    stats.cache_hits = cache_hits_.load();
    stats.cache_misses = cache_misses_.load();
    stats.cache_evictions = cache_evictions_.load();
    stats.total_errors = total_errors_.load();

    if (stats.total_requests > 0) {
        stats.cache_hit_rate =
            (static_cast<double>(stats.cache_hits) / stats.total_requests) * 100.0;
        stats.error_rate =
            (static_cast<double>(stats.total_errors) / stats.total_requests) * 100.0;
    } else {
        stats.cache_hit_rate = 0.0;
        stats.error_rate = 0.0;
    }

    stats.city_db_open = city_db_open_.load();
    stats.asn_db_open = asn_db_open_.load();
    stats.oui_db_open = oui_db_open_.load();

    {
        std::scoped_lock const lock(mutex_);
        if (!latencies_.empty()) {
            stats.avg_latency_ms =
                std::accumulate(latencies_.begin(), latencies_.end(), 0.0) / latencies_.size();
            stats.p50_latency_ms = calculate_percentile(0.50);
            stats.p95_latency_ms = calculate_percentile(0.95);
            stats.p99_latency_ms = calculate_percentile(0.99);
        } else {
            stats.avg_latency_ms = 0.0;
            stats.p50_latency_ms = 0.0;
            stats.p95_latency_ms = 0.0;
            stats.p99_latency_ms = 0.0;
        }
    }

    stats.current_qps = calculate_qps();

    stats.memory_usage_mb = 0;

    auto now = std::chrono::steady_clock::now();
    stats.uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();

    return stats;
}

void Metrics::reset() {
    std::scoped_lock const lock(mutex_);

    total_requests_ = 0;
    cache_hits_ = 0;
    cache_misses_ = 0;
    cache_evictions_ = 0;
    total_errors_ = 0;
    latencies_.clear();
    request_timestamps_.clear();
    start_time_ = std::chrono::steady_clock::now();

    LOG_INFO("Metrics reset");
}

void Metrics::set_city_db_status(bool open) {
    city_db_open_.store(open);
}

void Metrics::set_asn_db_status(bool open) {
    asn_db_open_.store(open);
}

void Metrics::set_oui_db_status(bool open) {
    oui_db_open_.store(open);
}

double Metrics::calculate_percentile(double percentile) const {
    if (latencies_.empty()) {
        return 0.0;
    }

    std::vector<double> sorted_latencies(latencies_.begin(), latencies_.end());
    std::ranges::sort(sorted_latencies);

    auto const index = static_cast<size_t>(percentile * (sorted_latencies.size() - 1));
    return sorted_latencies[index];
}

double Metrics::calculate_qps() const {
    std::scoped_lock const lock(mutex_);

    if (request_timestamps_.size() < 2) {
        return 0.0;
    }

    auto oldest = request_timestamps_.front();
    auto newest = request_timestamps_.back();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(newest.first - oldest.first).count();

    if (duration == 0) {
        return 0.0;
    }

    uint64_t const request_delta = newest.second - oldest.second;
    return (static_cast<double>(request_delta) / duration) * 1000.0;
}

}  // namespace ip_server
