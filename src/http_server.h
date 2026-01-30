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

// Forward declaration
class RateLimiter;
class APIAuth;
class Metrics;
class PasswordGenerator;

class IPGeoHTTPServer {
   public:
    using LookupHandler = std::function<LookupResult(const std::string&)>;

    explicit IPGeoHTTPServer(const std::string& host, uint16_t port, int thread_pool_size = 4,
                             bool enable_rate_limiter = true, int max_requests_per_minute = 100,
                             int max_batch_size = 100, bool enable_api_auth = false,
                             const std::string& api_keys_file   = "",
                             const std::string& default_api_key = "");
    ~IPGeoHTTPServer();

    IPGeoHTTPServer(const IPGeoHTTPServer&)            = delete;
    IPGeoHTTPServer& operator=(const IPGeoHTTPServer&) = delete;

    void set_lookup_handler(LookupHandler handler);
    void set_mac_lookup_handler(LookupHandler handler);

    // Get metrics collector
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

    // Cleanup thread for rate limiter
    void cleanup_thread_func(std::atomic<bool>& shutdown_requested);

    std::string host_;
    uint16_t port_;
    int thread_pool_size_;
    bool enable_rate_limiter_;
    int max_requests_per_minute_;
    int max_batch_size_;
    bool enable_api_auth_;
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