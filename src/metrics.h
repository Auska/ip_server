#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>

namespace ip_server {

class Metrics {
   public:
    Metrics();
    ~Metrics() = default;

    void record_request(bool cache_hit, double latency_ms);
    void record_error();
    void record_cache_eviction();

    struct Stats {
        uint64_t total_requests;
        uint64_t cache_hits;
        uint64_t cache_misses;
        uint64_t cache_evictions;
        uint64_t total_errors;

        double cache_hit_rate;
        double error_rate;

        double current_qps;
        double avg_latency_ms;
        double p50_latency_ms;
        double p95_latency_ms;
        double p99_latency_ms;

        size_t memory_usage_mb;
        uint64_t uptime_seconds;

        bool city_db_open;
        bool asn_db_open;
        bool oui_db_open;
    };

    Stats get_stats() const;

    void reset();

    void set_city_db_status(bool open);
    void set_asn_db_status(bool open);
    void set_oui_db_status(bool open);

   private:
    mutable std::mutex mutex_;

    std::atomic<uint64_t> total_requests_;
    std::atomic<uint64_t> cache_hits_;
    std::atomic<uint64_t> cache_misses_;
    std::atomic<uint64_t> cache_evictions_;
    std::atomic<uint64_t> total_errors_;

    std::deque<double> latencies_;

    std::chrono::steady_clock::time_point start_time_;
    std::deque<std::pair<std::chrono::steady_clock::time_point, uint64_t>> request_timestamps_;

    std::atomic<bool> city_db_open_;
    std::atomic<bool> asn_db_open_;
    std::atomic<bool> oui_db_open_;

    double calculate_percentile(double percentile) const;

    double calculate_qps() const;
};

}  // namespace ip_server
