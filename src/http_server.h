#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <string>

namespace ip_server {

class IPGeoHTTPServer {
public:
    using LookupHandler = std::function<nlohmann::json(const std::string&)>;

    explicit IPGeoHTTPServer(const std::string& host, uint16_t port, int thread_pool_size = 4);
    ~IPGeoHTTPServer() = default;

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
    httplib::Server server_;
    LookupHandler lookup_handler_;
};

} // namespace ip_server