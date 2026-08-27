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
      total_errors_(0),
      start_time_(std::chrono::steady_clock::now()) {
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
    request_timestamps_.push_back(now);

    auto cutoff = now - std::chrono::seconds(60);
    while (!request_timestamps_.empty() && request_timestamps_.front() < cutoff) {
        request_timestamps_.pop_front();
    }
}

void Metrics::record_error() {
    total_errors_++;
}

Metrics::Stats Metrics::get_stats() const {
    Stats stats{};

    stats.total_requests_  = total_requests_.load();
    stats.cache_hits_      = cache_hits_.load();
    stats.cache_misses_    = cache_misses_.load();
    stats.total_errors_    = total_errors_.load();

    if (stats.total_requests_ > 0) {
        stats.cache_hit_rate_ =
            (static_cast<double>(stats.cache_hits_) / stats.total_requests_) * 100.0;
        stats.error_rate_ =
            (static_cast<double>(stats.total_errors_) / stats.total_requests_) * 100.0;
    }

    stats.city_db_open_ = city_db_open_;
    stats.asn_db_open_  = asn_db_open_;
    stats.oui_db_open_  = oui_db_open_;

    {
        std::scoped_lock const lock(mutex_);
        if (!latencies_.empty()) {
            stats.avg_latency_ms_ =
                std::accumulate(latencies_.begin(), latencies_.end(), 0.0) / latencies_.size();
            stats.p50_latency_ms_ = calculate_percentile(0.50);
            stats.p95_latency_ms_ = calculate_percentile(0.95);
            stats.p99_latency_ms_ = calculate_percentile(0.99);
        }
    }

    stats.current_qps_ = calculate_qps();

    auto now = std::chrono::steady_clock::now();
    stats.uptime_seconds_ =
        std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();

    return stats;
}

void Metrics::set_db_status(bool city_open, bool asn_open, bool oui_open) {
    city_db_open_ = city_open;
    asn_db_open_  = asn_open;
    oui_db_open_  = oui_open;
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

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        request_timestamps_.back() - request_timestamps_.front())
                        .count();

    if (duration == 0) {
        return 0.0;
    }

    return (static_cast<double>(request_timestamps_.size() - 1) / duration) * 1000.0;
}

}  // namespace ip_server
