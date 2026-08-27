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
    Metrics* metrics_;
};

}  // namespace ip_server
