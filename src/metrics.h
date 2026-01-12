#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <deque>

namespace ip_server {

class Metrics {
public:
    Metrics();
    ~Metrics() = default;

    // Request tracking
    void record_request(bool cache_hit, double latency_ms);

    // Get current metrics
    struct Stats {
        // Request counts
        uint64_t total_requests;
        uint64_t cache_hits;
        uint64_t cache_misses;
        
        // Cache statistics
        double cache_hit_rate;
        
        // Performance
        double current_qps;
        double avg_latency_ms;
        double p50_latency_ms;
        double p95_latency_ms;
        double p99_latency_ms;
        
        // System
        size_t memory_usage_mb;
        
        // Database status
        bool city_db_open;
        bool asn_db_open;
        bool oui_db_open;
    };
    
    Stats get_stats() const;
    
    // Reset metrics
    void reset();
    
    // Set database status
    void set_city_db_status(bool open);
    void set_asn_db_status(bool open);
    void set_oui_db_status(bool open);

private:
    mutable std::mutex mutex_;
    
    // Request counters
    std::atomic<uint64_t> total_requests_;
    std::atomic<uint64_t> cache_hits_;
    std::atomic<uint64_t> cache_misses_;
    
    // Latency tracking (last 1000 requests)
    std::deque<double> latencies_;
    static constexpr size_t MAX_LATENCIES = 1000;
    
    // QPS calculation
    std::chrono::steady_clock::time_point start_time_;
    std::deque<std::pair<std::chrono::steady_clock::time_point, uint64_t>> request_timestamps_;
    
    // Database status
    std::atomic<bool> city_db_open_;
    std::atomic<bool> asn_db_open_;
    std::atomic<bool> oui_db_open_;
    
    // Calculate percentiles
    double calculate_percentile(double percentile) const;
    
    // Calculate QPS
    double calculate_qps() const;
};

} // namespace ip_server