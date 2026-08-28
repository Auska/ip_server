#pragma once

#include <httplib.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

#include "cache.h"
#include "config.h"
#include "password_handler.h"
#include "types.h"

namespace ip_server {

class RateLimiter;
class APIAuth;
class Metrics;

namespace constants {
constexpr int CLEANUP_INTERVAL_SECONDS = 300;
}  // namespace constants

class IPGeoHTTPServer {
   public:
    using LookupHandler = std::function<LookupResult(const std::string&)>;

    explicit IPGeoHTTPServer(const ServerConfig& config);
    ~IPGeoHTTPServer();

    IPGeoHTTPServer(const IPGeoHTTPServer&) = delete;
    IPGeoHTTPServer& operator=(const IPGeoHTTPServer&) = delete;

    void set_lookup_handler(LookupHandler handler);
    void set_mac_lookup_handler(LookupHandler handler);
    void set_cache_stats_handler(std::function<CacheStats()> handler);

    Metrics* get_metrics() { return metrics_.get(); }

    bool start(std::atomic<bool>& shutdown_requested);
    void stop();

   private:
    void setup_routes();
    void setup_cors();
    bool authenticate_request(const httplib::Request& req, httplib::Response& res);

    void handle_root(const httplib::Request& req, httplib::Response& res);
    void handle_health(const httplib::Request& req, httplib::Response& res);
    void handle_lookup_get(const httplib::Request& req, httplib::Response& res);
    void handle_lookup_post(const httplib::Request& req, httplib::Response& res);

    [[nodiscard]] std::optional<std::string> prepare_request(const httplib::Request& req, httplib::Response& res);

    std::string get_real_client_ip(const httplib::Request& req) const;

    void cleanup_thread_func(std::stop_token stop_token);
    void log_startup_banner() const;
    bool start_server_and_wait(std::atomic<bool>& shutdown_requested);

    std::string host_;
    uint16_t port_;
    int thread_pool_size_;
    bool enable_rate_limiter_;
    int max_batch_size_;
    bool enable_api_auth_;
    httplib::Server server_;
    LookupHandler lookup_handler_;
    LookupHandler mac_lookup_handler_;
    std::function<CacheStats()> cache_stats_handler_;
    std::unique_ptr<RateLimiter> rate_limiter_;
    std::unique_ptr<APIAuth> api_auth_;
    std::unique_ptr<Metrics> metrics_;
    PasswordHandler password_handler_;
    std::jthread cleanup_thread_;
};

}  // namespace ip_server
