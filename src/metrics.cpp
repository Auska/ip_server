#include "metrics.h"
#include "logger.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace ip_server {

Metrics::Metrics()
    : total_requests_(0),
      cache_hits_(0),
      cache_misses_(0),
      start_time_(std::chrono::steady_clock::now()),
      city_db_open_(false),
      asn_db_open_(false) {
    LOG_INFO("Metrics collector initialized");
}

void Metrics::record_request(bool cache_hit, double latency_ms) {
    // Update counters
    total_requests_++;
    if (cache_hit) {
        cache_hits_++;
    } else {
        cache_misses_++;
    }
    
    // Record latency
    std::lock_guard<std::mutex> lock(mutex_);
    latencies_.push_back(latency_ms);
    if (latencies_.size() > MAX_LATENCIES) {
        latencies_.pop_front();
    }
    
    // Record timestamp for QPS calculation
    auto now = std::chrono::steady_clock::now();
    request_timestamps_.push_back({now, total_requests_.load()});
    
    // Keep only last 60 seconds of timestamps
    auto cutoff = now - std::chrono::seconds(60);
    while (!request_timestamps_.empty() && request_timestamps_.front().first < cutoff) {
        request_timestamps_.pop_front();
    }
}

Metrics::Stats Metrics::get_stats() const {
    Stats stats;
    
    // Basic counters
    stats.total_requests = total_requests_.load();
    stats.cache_hits = cache_hits_.load();
    stats.cache_misses = cache_misses_.load();
    
    // Cache hit rate
    if (stats.total_requests > 0) {
        stats.cache_hit_rate = (static_cast<double>(stats.cache_hits) / stats.total_requests) * 100.0;
    } else {
        stats.cache_hit_rate = 0.0;
    }
    
    // Database status
    stats.city_db_open = city_db_open_.load();
    stats.asn_db_open = asn_db_open_.load();
    
    // Latency statistics
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!latencies_.empty()) {
            stats.avg_latency_ms = std::accumulate(latencies_.begin(), latencies_.end(), 0.0) / latencies_.size();
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
    
    // QPS
    stats.current_qps = calculate_qps();
    
    // Memory usage (approximate)
    stats.memory_usage_mb = 0; // TODO: Implement actual memory tracking
    
    return stats;
}

void Metrics::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    total_requests_ = 0;
    cache_hits_ = 0;
    cache_misses_ = 0;
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

double Metrics::calculate_percentile(double percentile) const {
    if (latencies_.empty()) {
        return 0.0;
    }
    
    std::vector<double> sorted_latencies(latencies_.begin(), latencies_.end());
    std::sort(sorted_latencies.begin(), sorted_latencies.end());
    
    size_t index = static_cast<size_t>(percentile * (sorted_latencies.size() - 1));
    return sorted_latencies[index];
}

double Metrics::calculate_qps() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (request_timestamps_.size() < 2) {
        return 0.0;
    }
    
    auto oldest = request_timestamps_.front();
    auto newest = request_timestamps_.back();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        newest.first - oldest.first
    ).count();
    
    if (duration == 0) {
        return 0.0;
    }
    
    uint64_t request_delta = newest.second - oldest.second;
    return (static_cast<double>(request_delta) / duration) * 1000.0;
}

} // namespace ip_server