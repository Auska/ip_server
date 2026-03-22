#pragma once

#include <httplib.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "types.h"

namespace ip_server {

class RateLimiter;
class APIAuth;
class Metrics;
class PasswordGenerator;

namespace constants {
constexpr int DEFAULT_THREAD_POOL_SIZE = 4;
constexpr int DEFAULT_MAX_REQUESTS_PER_MINUTE = 100;
constexpr int DEFAULT_MAX_BATCH_SIZE = 100;
constexpr int MAX_PASSWORD_BATCH = 100;
constexpr int CLEANUP_INTERVAL_SECONDS = 300;
constexpr int SHUTDOWN_TIMEOUT_SECONDS = 30;
constexpr size_t MAX_LATENCIES = 1000;
constexpr size_t DEFAULT_RATE_LIMITER_MAX_IPS = 10000;
}  // namespace constants

class IPGeoHTTPServer {
   public:
    using LookupHandler = std::function<LookupResult(const std::string&)>;

    explicit IPGeoHTTPServer(const std::string& host, uint16_t port,
                             int thread_pool_size = constants::DEFAULT_THREAD_POOL_SIZE,
                             bool enable_rate_limiter = true,
                             int max_requests_per_minute = constants::DEFAULT_MAX_REQUESTS_PER_MINUTE,
                             int max_batch_size = constants::DEFAULT_MAX_BATCH_SIZE,
                             bool enable_api_auth = false,
                             const std::string& api_keys_file = "",
                             const std::string& default_api_key = "",
                             const std::vector<std::string>& trusted_proxies = {});
    ~IPGeoHTTPServer();

    IPGeoHTTPServer(const IPGeoHTTPServer&) = delete;
    IPGeoHTTPServer& operator=(const IPGeoHTTPServer&) = delete;

    void set_lookup_handler(LookupHandler handler);
    void set_mac_lookup_handler(LookupHandler handler);

    Metrics* get_metrics() { return metrics_.get(); }

    bool start(std::atomic<bool>& shutdown_requested);
    void stop();
    bool is_running() const;

   private:
    void setup_routes();
    void setup_cors();
    bool authenticate_request(const httplib::Request& req, httplib::Response& res);
    void send_error_response(httplib::Response& res, int status, const std::string& error,
                             const std::string& message);
    void send_json_response(httplib::Response& res, const nlohmann::json& data, int status = 200);

    std::string get_real_client_ip(const httplib::Request& req) const;
    bool is_trusted_proxy(const std::string& ip) const;

    void cleanup_thread_func(std::atomic<bool>& shutdown_requested);

    std::string host_;
    uint16_t port_;
    int thread_pool_size_;
    bool enable_rate_limiter_;
    int max_batch_size_;
    bool enable_api_auth_;
    std::vector<std::string> trusted_proxies_;
    httplib::Server server_;
    LookupHandler lookup_handler_;
    LookupHandler mac_lookup_handler_;
    std::unique_ptr<RateLimiter> rate_limiter_;
    std::unique_ptr<APIAuth> api_auth_;
    std::unique_ptr<Metrics> metrics_;
    std::unique_ptr<PasswordGenerator> password_generator_;
    std::jthread cleanup_thread_;
    std::atomic<bool> cleanup_thread_running_;
};

}  // namespace ip_server
