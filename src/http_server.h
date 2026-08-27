#pragma once

#include <httplib.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "types.h"
#include "password_handler.h"

namespace ip_server {

class RateLimiter;
class APIAuth;
class Metrics;
class PasswordHandler;

namespace constants {
constexpr int DEFAULT_THREAD_POOL_SIZE = 4;
constexpr int DEFAULT_MAX_REQUESTS_PER_MINUTE = 100;
constexpr int DEFAULT_MAX_BATCH_SIZE = 100;
constexpr int CLEANUP_INTERVAL_SECONDS = 300;
constexpr size_t DEFAULT_RATE_LIMITER_MAX_IPS = 10000;
}  // namespace constants

// Shared JSON response helpers (used by IPGeoHTTPServer and PasswordHandler)
inline void send_json_response(httplib::Response& res, const nlohmann::json& data,
                               int status = 200) {
    res.status = status;
    res.set_content(data.dump(), "application/json");
}

inline void send_error_response(httplib::Response& res, int status, const std::string& error,
                                const std::string& message) {
    nlohmann::json error_json;
    error_json["error"]   = error;
    error_json["message"] = message;
    res.status            = status;
    res.set_content(error_json.dump(), "application/json");
}

class IPGeoHTTPServer {
   public:
    using LookupHandler = std::function<LookupResult(const std::string&)>;

    explicit IPGeoHTTPServer(std::string  host, uint16_t port,
                             int thread_pool_size = constants::DEFAULT_THREAD_POOL_SIZE,
                             bool enable_rate_limiter = true,
                             int max_requests_per_minute = constants::DEFAULT_MAX_REQUESTS_PER_MINUTE,
                             int max_batch_size = constants::DEFAULT_MAX_BATCH_SIZE,
                             bool enable_api_auth = false,
                             const std::string& api_keys_file = "",
                             const std::string& default_api_key = "");
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

    void handle_root(const httplib::Request& req, httplib::Response& res);
    void handle_health(const httplib::Request& req, httplib::Response& res);
    void handle_lookup_get(const httplib::Request& req, httplib::Response& res);
    void handle_lookup_post(const httplib::Request& req, httplib::Response& res);

    [[nodiscard]] std::optional<std::string> prepare_request(const httplib::Request& req, httplib::Response& res);

    std::string get_real_client_ip(const httplib::Request& req) const;

    void cleanup_thread_func(std::atomic<bool>& shutdown_requested);
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
    std::unique_ptr<RateLimiter> rate_limiter_;
    std::unique_ptr<APIAuth> api_auth_;
    std::unique_ptr<Metrics> metrics_;
    PasswordHandler password_handler_;
    std::jthread cleanup_thread_;
};

}  // namespace ip_server
