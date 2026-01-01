#include "http_server.h"
#include "rate_limiter.h"
#include "logger.h"
#include <stdexcept>
#include <thread>
#include <chrono>
#include <atomic>

namespace ip_server {

IPGeoHTTPServer::IPGeoHTTPServer(const std::string& host, uint16_t port, int thread_pool_size,
                                bool enable_rate_limiter, int max_requests_per_minute,
                                int max_batch_size)
    : host_(host), port_(port), thread_pool_size_(thread_pool_size),
      enable_rate_limiter_(enable_rate_limiter), max_requests_per_minute_(max_requests_per_minute),
      max_batch_size_(max_batch_size) {

    if (enable_rate_limiter_) {
        rate_limiter_ = std::make_unique<RateLimiter>(max_requests_per_minute, std::chrono::seconds(60));
        LOG_INFO("Rate limiter enabled: " + std::to_string(max_requests_per_minute) + " requests per minute");
    }

    LOG_INFO("HTTP server configured for " + host_ + ":" + std::to_string(port_) + 
            " with " + std::to_string(thread_pool_size_) + " threads");
    LOG_INFO("Batch size limit: " + std::to_string(max_batch_size_) + " IPs per request");
}

IPGeoHTTPServer::~IPGeoHTTPServer() = default;

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
        auto client_ip = req.remote_addr;
        if (client_ip.empty()) {
            res.status = 400;
            nlohmann::json error;
            error["error"] = "Unable to determine source IP address";
            res.set_content(error.dump(), "application/json");
            LOG_WARNING("Lookup request: unable to determine source IP");
            return;
        }

        // Check rate limit
        if (enable_rate_limiter_ && !rate_limiter_->is_allowed(client_ip)) {
            res.status = 429;
            nlohmann::json error;
            error["error"] = "Rate limit exceeded";
            error["remaining"] = rate_limiter_->get_remaining(client_ip);
            res.set_header("Retry-After", "60");
            res.set_content(error.dump(), "application/json");
            LOG_WARNING("Rate limit exceeded for IP: " + client_ip);
            return;
        }

        auto ip_param = req.get_param_value("ip");
        
        // If no IP parameter provided, use the source IP address
        if (ip_param.empty()) {
            ip_param = client_ip;
            LOG_DEBUG("Lookup request using source IP: " + ip_param);
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
        auto client_ip = req.remote_addr;
        if (client_ip.empty()) {
            res.status = 400;
            nlohmann::json error;
            error["error"] = "Unable to determine source IP address";
            res.set_content(error.dump(), "application/json");
            LOG_WARNING("Batch lookup request: unable to determine source IP");
            return;
        }

        // Check rate limit
        if (enable_rate_limiter_ && !rate_limiter_->is_allowed(client_ip)) {
            res.status = 429;
            nlohmann::json error;
            error["error"] = "Rate limit exceeded";
            error["remaining"] = rate_limiter_->get_remaining(client_ip);
            res.set_header("Retry-After", "60");
            res.set_content(error.dump(), "application/json");
            LOG_WARNING("Rate limit exceeded for IP: " + client_ip);
            return;
        }

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

            // Check batch size limit
            size_t batch_size = body["ips"].size();
            if (batch_size > static_cast<size_t>(max_batch_size_)) {
                res.status = 400;
                nlohmann::json error;
                error["error"] = "Batch size exceeds maximum limit";
                error["max_batch_size"] = max_batch_size_;
                error["requested_size"] = batch_size;
                res.set_content(error.dump(), "application/json");
                LOG_WARNING("Batch lookup request exceeds size limit: " + std::to_string(batch_size) + 
                           " > " + std::to_string(max_batch_size_));
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

    // Configure thread pool
    server_.new_task_queue = [this] {
        return new httplib::ThreadPool(thread_pool_size_);
    };

    LOG_INFO("Starting HTTP server on " + host_ + ":" + std::to_string(port_));
    std::cout << "\n========================================" << std::endl;
    std::cout << "  IP Geolocation & AS Lookup Service" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Server: http://" << host_ << ":" << port_ << std::endl;
    std::cout << "Thread Pool: " << thread_pool_size_ << " threads" << std::endl;
    std::cout << "\nAPI Endpoints:" << std::endl;
    std::cout << "  GET  /                       - Service info" << std::endl;
    std::cout << "  GET  /health                 - Health check" << std::endl;
    std::cout << "  GET  /lookup[?ip=<address>]   - IP lookup (uses source IP if no param)" << std::endl;
    std::cout << "  POST /lookup                 - Batch lookup" << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // Start server in a separate thread to allow graceful shutdown
    std::atomic<bool> running(true);
    std::thread server_thread([this, &running]() {
        if (!server_.listen(host_.c_str(), port_)) {
            LOG_ERROR("Failed to start HTTP server");
            running.store(false);
        }
    });

    // Wait for server to start or fail
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!running.load()) {
        if (server_thread.joinable()) {
            server_thread.join();
        }
        return false;
    }

    // Wait for shutdown signal from main
    while (running.load() && server_.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop server and wait for thread to finish
    server_.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }

    return true;
}

void IPGeoHTTPServer::stop() {
    LOG_INFO("Stopping HTTP server");
    server_.stop();
}

} // namespace ip_server