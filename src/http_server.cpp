#include "http_server.h"
#include "logger.h"
#include <stdexcept>

namespace ip_server {

IPGeoHTTPServer::IPGeoHTTPServer(const std::string& host, uint16_t port)
    : host_(host), port_(port) {
    LOG_INFO("HTTP server configured for " + host_ + ":" + std::to_string(port_));
}

void IPGeoHTTPServer::set_lookup_handler(LookupHandler handler) {
    lookup_handler_ = std::move(handler);
    LOG_INFO("Lookup handler registered");
}

void IPGeoHTTPServer::setup_cors() {
    server_.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });

    server_.Options(".*", [](const httplib::Request&, httplib::Response&) {
        return;
    });
}

void IPGeoHTTPServer::setup_routes() {
    // Root endpoint - API info
    server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json info;
        info["service"] = "IP Geolocation & AS Lookup Service";
        info["version"] = "2.0.0";
        info["endpoints"] = nlohmann::json::array({"/", "/lookup", "/health"});
        res.set_content(info.dump(2), "application/json");
    });

    // Health check endpoint
    server_.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json health;
        health["status"] = "ok";
        health["timestamp"] = std::time(nullptr);
        res.set_content(health.dump(), "application/json");
    });

    // Single IP lookup endpoint
    server_.Get("/lookup", [this](const httplib::Request& req, httplib::Response& res) {
        auto ip_param = req.get_param_value("ip");
        if (ip_param.empty()) {
            res.status = 400;
            nlohmann::json error;
            error["error"] = "Missing 'ip' parameter";
            res.set_content(error.dump(), "application/json");
            LOG_WARNING("Lookup request missing IP parameter");
            return;
        }

        try {
            LOG_DEBUG("Lookup request for IP: " + ip_param);
            auto result = lookup_handler_(ip_param);
            res.set_content(result.dump(2), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json error;
            error["error"] = e.what();
            res.set_content(error.dump(), "application/json");
            LOG_ERROR(std::string("Error processing lookup for ") + ip_param + ": " + e.what());
        }
    });

    // Batch lookup endpoint
    server_.Post("/lookup", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);

            if (!body.contains("ips") || !body["ips"].is_array()) {
                res.status = 400;
                nlohmann::json error;
                error["error"] = "Missing or invalid 'ips' array in request body";
                res.set_content(error.dump(), "application/json");
                LOG_WARNING("Batch lookup request missing 'ips' array");
                return;
            }

            nlohmann::json results = nlohmann::json::array();
            for (const auto& ip : body["ips"]) {
                if (ip.is_string()) {
                    auto ip_str = ip.get<std::string>();
                    LOG_DEBUG("Batch lookup for IP: " + ip_str);
                    results.push_back(lookup_handler_(ip_str));
                }
            }

            LOG_INFO("Batch lookup completed for " + std::to_string(results.size()) + " IPs");
            res.set_content(results.dump(2), "application/json");

        } catch (const nlohmann::json::exception& e) {
            res.status = 400;
            nlohmann::json error;
            error["error"] = "Invalid JSON: " + std::string(e.what());
            res.set_content(error.dump(), "application/json");
            LOG_WARNING("Invalid JSON in batch lookup request: " + std::string(e.what()));
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json error;
            error["error"] = e.what();
            res.set_content(error.dump(), "application/json");
            LOG_ERROR(std::string("Error processing batch lookup: ") + e.what());
        }
    });

    LOG_INFO("HTTP routes configured");
}

bool IPGeoHTTPServer::start() {
    if (!lookup_handler_) {
        LOG_ERROR("Cannot start server: lookup handler not set");
        return false;
    }

    setup_cors();
    setup_routes();

    LOG_INFO("Starting HTTP server on " + host_ + ":" + std::to_string(port_));
    std::cout << "\n========================================" << std::endl;
    std::cout << "  IP Geolocation & AS Lookup Service" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Server: http://" << host_ << ":" << port_ << std::endl;
    std::cout << "\nAPI Endpoints:" << std::endl;
    std::cout << "  GET  /                       - Service info" << std::endl;
    std::cout << "  GET  /health                 - Health check" << std::endl;
    std::cout << "  GET  /lookup?ip=<address>     - Single IP lookup" << std::endl;
    std::cout << "  POST /lookup                 - Batch lookup" << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server" << std::endl;
    std::cout << "========================================\n" << std::endl;

    if (!server_.listen(host_.c_str(), port_)) {
        LOG_ERROR("Failed to start HTTP server");
        return false;
    }

    return true;
}

void IPGeoHTTPServer::stop() {
    LOG_INFO("Stopping HTTP server");
    server_.stop();
}

} // namespace ip_server