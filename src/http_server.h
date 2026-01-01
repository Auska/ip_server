#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include <memory>
#include <chrono>

namespace ip_server {

// Forward declaration
class RateLimiter;

class IPGeoHTTPServer {
public:
    using LookupHandler = std::function<nlohmann::json(const std::string&)>;

    explicit IPGeoHTTPServer(const std::string& host, uint16_t port, int thread_pool_size = 4,
                            bool enable_rate_limiter = true, int max_requests_per_minute = 100,
                            int max_batch_size = 100);
    ~IPGeoHTTPServer();

    IPGeoHTTPServer(const IPGeoHTTPServer&) = delete;
    IPGeoHTTPServer& operator=(const IPGeoHTTPServer&) = delete;

    void set_lookup_handler(LookupHandler handler);

    bool start();
    void stop();

private:
    void setup_routes();
    void setup_cors();

    std::string host_;
    uint16_t port_;
    int thread_pool_size_;
    bool enable_rate_limiter_;
    int max_requests_per_minute_;
    int max_batch_size_;
    httplib::Server server_;
    LookupHandler lookup_handler_;
    std::unique_ptr<RateLimiter> rate_limiter_;
};

} // namespace ip_server