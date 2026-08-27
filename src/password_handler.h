#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>

#include "metrics.h"

namespace ip_server {

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

/// Standalone password generation HTTP handler.
/// Extracted from IPGeoHTTPServer to reduce god-class size.
class PasswordHandler {
   public:
    explicit PasswordHandler(Metrics* metrics);

    void handle_get(const httplib::Request& req, httplib::Response& res);
    void handle_post(const httplib::Request& req, httplib::Response& res);

   private:
    Metrics* metrics_;
};

}  // namespace ip_server
