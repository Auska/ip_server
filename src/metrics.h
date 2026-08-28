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

    void record_request(double latency_ms);
    void record_error();

    struct Stats {
        uint64_t total_requests_;
        uint64_t total_errors_;

        double error_rate_;

        double current_qps_;
        double avg_latency_ms_;
        double p50_latency_ms_;
        double p95_latency_ms_;
        double p99_latency_ms_;

        uint64_t uptime_seconds_;

        bool city_db_open_;
        bool asn_db_open_;
        bool oui_db_open_;
    };

    Stats get_stats() const;

    void set_db_status(bool city_open, bool asn_open, bool oui_open);

   private:
    mutable std::mutex mutex_;

    std::atomic<uint64_t> total_requests_;
    std::atomic<uint64_t> total_errors_;

    std::deque<double> latencies_;

    std::chrono::steady_clock::time_point start_time_;
    std::deque<std::chrono::steady_clock::time_point> request_timestamps_;

    bool city_db_open_{false};
    bool asn_db_open_{false};
    bool oui_db_open_{false};

    double calculate_percentile(double percentile) const;

    double calculate_qps() const;
};

}  // namespace ip_server
