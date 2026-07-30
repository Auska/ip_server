#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <memory>

#include "metrics.h"

namespace ip_server {

/// Standalone password generation HTTP handler.
/// Extracted from IPGeoHTTPServer to reduce god-class size.
class PasswordHandler {
   public:
    explicit PasswordHandler(Metrics* metrics);

    void handle_get(const httplib::Request& req, httplib::Response& res);
    void handle_post(const httplib::Request& req, httplib::Response& res);

   private:
    static void send_error_response(httplib::Response& res, int status,
                                    const std::string& error, const std::string& message);
    static void send_json_response(httplib::Response& res, const nlohmann::json& data,
                                   int status = 200);

    Metrics* metrics_;
};

}  // namespace ip_server
