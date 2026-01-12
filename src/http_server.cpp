#include "http_server.h"
#include "rate_limiter.h"
#include "auth.h"
#include "metrics.h"
#include "logger.h"
#include <stdexcept>
#include <thread>
#include <chrono>
#include <atomic>
#include <future>
#include <vector>

namespace ip_server {

IPGeoHTTPServer::IPGeoHTTPServer(const std::string& host, uint16_t port, int thread_pool_size,
                                bool enable_rate_limiter, int max_requests_per_minute,
                                int max_batch_size, bool enable_api_auth,
                                const std::string& api_keys_file,
                                const std::string& default_api_key)
    : host_(host), port_(port), thread_pool_size_(thread_pool_size),
      enable_rate_limiter_(enable_rate_limiter), max_requests_per_minute_(max_requests_per_minute),
      max_batch_size_(max_batch_size), enable_api_auth_(enable_api_auth) {

    // Initialize metrics collector
    metrics_ = std::make_unique<Metrics>();

    if (enable_rate_limiter_) {
        // Create rate limiter with max 10,000 IP records
        rate_limiter_ = std::make_unique<RateLimiter>(
            max_requests_per_minute,
            std::chrono::seconds(60),
            10000  // max_ip_records
        );
        LOG_INFO("Rate limiter enabled: " + std::to_string(max_requests_per_minute) + " requests per minute");
        LOG_INFO("Rate limiter max IP records: 10000");
    }

    if (enable_api_auth_) {
        api_auth_ = std::make_unique<APIAuth>(true);
        
        // Load API keys from file if specified
        if (!api_keys_file.empty()) {
            if (api_auth_->load_keys_from_file(api_keys_file)) {
                LOG_INFO("Loaded " + std::to_string(api_auth_->key_count()) + " API keys from file");
            }
        }
        
        // Add default API key if specified
        if (!default_api_key.empty()) {
            api_auth_->add_key(default_api_key);
            LOG_INFO("Added default API key");
        }
        
        if (api_auth_->key_count() == 0) {
            LOG_WARNING("API authentication enabled but no API keys configured!");
        }
    }

    LOG_INFO("HTTP server configured for " + host_ + ":" + std::to_string(port_) + 
            " with " + std::to_string(thread_pool_size_) + " threads");
    LOG_INFO("Batch size limit: " + std::to_string(max_batch_size_) + " IPs per request");
    LOG_INFO("API Auth: " + std::string(enable_api_auth_ ? "enabled" : "disabled"));
}

IPGeoHTTPServer::~IPGeoHTTPServer() = default;

void IPGeoHTTPServer::set_lookup_handler(LookupHandler handler) {
    lookup_handler_ = std::move(handler);
    LOG_INFO("Lookup handler registered");
}

void IPGeoHTTPServer::set_mac_lookup_handler(LookupHandler handler) {
    mac_lookup_handler_ = std::move(handler);
    LOG_INFO("MAC lookup handler registered");
}

bool IPGeoHTTPServer::authenticate_request(const httplib::Request& req, httplib::Response& res) {
    if (!enable_api_auth_ || !api_auth_) {
        return true; // Authentication disabled, allow all requests
    }

    // Check for API key in Authorization header
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) {
        send_error_response(res, 401, "Unauthorized", "Missing API key in Authorization header");
        res.set_header("WWW-Authenticate", "Bearer");
        LOG_WARNING("Unauthorized request: missing API key");
        return false;
    }

    // Extract API key from "Bearer <key>" format
    std::string prefix = "Bearer ";
    if (auth_header.substr(0, prefix.length()) != prefix) {
        send_error_response(res, 401, "Unauthorized", "Invalid Authorization header format. Expected: Bearer <api_key>");
        res.set_header("WWW-Authenticate", "Bearer");
        LOG_WARNING("Unauthorized request: invalid Authorization header format");
        return false;
    }

    std::string api_key = auth_header.substr(prefix.length());
    if (!api_auth_->is_valid(api_key)) {
        send_error_response(res, 401, "Unauthorized", "Invalid API key");
        LOG_WARNING("Unauthorized request: invalid API key from " + req.remote_addr);
        return false;
    }

    return true;
}

void IPGeoHTTPServer::send_error_response(httplib::Response& res, int status, const std::string& error, const std::string& message) {
    nlohmann::json error_json;
    error_json["error"] = error;
    error_json["message"] = message;
    res.status = status;
    res.set_content(error_json.dump(), "application/json");
}

void IPGeoHTTPServer::send_json_response(httplib::Response& res, const nlohmann::json& data, int status) {
    res.status = status;
    res.set_content(data.dump(), "application/json");
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
        info["description"] = "Use /lookup with 'ip=' or 'mac=' parameter for single queries, or 'ips'/'macs' array for batch queries";
        res.set_content(info.dump(), "application/json");
    });

    // Health check endpoint
    server_.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        auto stats = metrics_->get_stats();
        
        nlohmann::json health;
        health["status"] = "ok";
        health["timestamp"] = std::time(nullptr);
        
        // Database status
        health["databases"] = {
            {"city", {{"open", stats.city_db_open}}},
            {"asn", {{"open", stats.asn_db_open}}},
            {"oui", {{"open", stats.oui_db_open}}}
        };
        
        // Performance metrics
        health["metrics"] = {
            {"total_requests", stats.total_requests},
            {"qps", round(stats.current_qps * 100) / 100},
            {"avg_latency_ms", round(stats.avg_latency_ms * 1000) / 1000},
            {"p50_latency_ms", round(stats.p50_latency_ms * 1000) / 1000},
            {"p95_latency_ms", round(stats.p95_latency_ms * 1000) / 1000},
            {"p99_latency_ms", round(stats.p99_latency_ms * 1000) / 1000}
        };
        
        // Cache metrics
        health["cache"] = {
            {"hits", stats.cache_hits},
            {"misses", stats.cache_misses},
            {"hit_rate_percent", round(stats.cache_hit_rate * 100) / 100}
        };
        
        // System metrics
        health["system"] = {
            {"memory_usage_mb", stats.memory_usage_mb}
        };
        
        res.set_content(health.dump(), "application/json");
    });

    // Single lookup endpoint (supports both IP and MAC queries)
    server_.Get("/lookup", [this](const httplib::Request& req, httplib::Response& res) {
        // Check authentication
        if (!authenticate_request(req, res)) {
            return;
        }

        auto client_ip = req.remote_addr;
        if (client_ip.empty()) {
            res.status = 400;
            send_error_response(res, 400, "Bad Request", "Unable to determine source IP address");
            LOG_WARNING("Lookup request: unable to determine source IP");
            return;
        }

        // Check rate limit
        if (enable_rate_limiter_ && !rate_limiter_->is_allowed(client_ip)) {
            nlohmann::json error;
            error["error"] = "Rate limit exceeded";
            error["remaining"] = rate_limiter_->get_remaining(client_ip);
            send_json_response(res, error, 429);
            res.set_header("Retry-After", "60");
            LOG_WARNING("Rate limit exceeded for IP: " + client_ip);
            return;
        }

        auto mac_param = req.get_param_value("mac");
        auto ip_param = req.get_param_value("ip");

        // If both parameters are missing, use the source IP address
        if (mac_param.empty() && ip_param.empty()) {
            ip_param = client_ip;
            LOG_DEBUG("Lookup request using source IP: " + ip_param);
        }

        // If both parameters are provided, return an error
        if (!mac_param.empty() && !ip_param.empty()) {
            send_error_response(res, 400, "Bad Request", "Cannot specify both 'ip' and 'mac' parameters simultaneously");
            LOG_WARNING("Lookup request: both ip and mac parameters provided");
            return;
        }

        try {
            // Determine query type based on parameter
            if (!mac_param.empty()) {
                // MAC address lookup
                if (!mac_lookup_handler_) {
                    send_error_response(res, 500, "Internal Server Error", "MAC lookup handler not configured");
                    LOG_WARNING("MAC lookup requested but handler not set");
                    return;
                }

                LOG_DEBUG("MAC lookup request for: " + mac_param);
                auto result = mac_lookup_handler_(mac_param);

                // Record metrics
                metrics_->record_request(result.cache_hit, result.latency_ms);

                res.set_content(result.data.dump(), "application/json");
            } else {
                // IP address lookup
                if (!lookup_handler_) {
                    send_error_response(res, 500, "Internal Server Error", "IP lookup handler not configured");
                    LOG_WARNING("IP lookup requested but handler not set");
                    return;
                }

                LOG_DEBUG("IP lookup request for: " + ip_param);
                auto result = lookup_handler_(ip_param);

                // Record metrics
                metrics_->record_request(result.cache_hit, result.latency_ms);

                res.set_content(result.data.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json error;
            error["error"] = e.what();
            res.set_content(error.dump(), "application/json");
            std::string query_param = mac_param.empty() ? ip_param : mac_param;
            LOG_ERROR(std::string("Error processing lookup for ") + query_param + ": " + e.what());
        }
    });

    // Batch lookup endpoint (supports both IP and MAC queries)
    server_.Post("/lookup", [this](const httplib::Request& req, httplib::Response& res) {
        // Check authentication
        if (!authenticate_request(req, res)) {
            return;
        }

        auto client_ip = req.remote_addr;
        if (client_ip.empty()) {
            res.status = 400;
            send_error_response(res, 400, "Bad Request", "Unable to determine source IP address");
            LOG_WARNING("Batch lookup request: unable to determine source IP");
            return;
        }

        // Check rate limit
        if (enable_rate_limiter_ && !rate_limiter_->is_allowed(client_ip)) {
            nlohmann::json error;
            error["error"] = "Rate limit exceeded";
            error["remaining"] = rate_limiter_->get_remaining(client_ip);
            send_json_response(res, error, 429);
            res.set_header("Retry-After", "60");
            LOG_WARNING("Rate limit exceeded for IP: " + client_ip);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);

            // Determine query type based on request body
            bool has_ips = body.contains("ips") && body["ips"].is_array();
            bool has_macs = body.contains("macs") && body["macs"].is_array();

            if (!has_ips && !has_macs) {
                send_error_response(res, 400, "Bad Request", "Missing or invalid 'ips' or 'macs' array in request body");
                LOG_WARNING("Batch lookup request missing 'ips' or 'macs' array");
                return;
            }

            if (has_ips && has_macs) {
                send_error_response(res, 400, "Bad Request", "Cannot specify both 'ips' and 'macs' in the same request");
                LOG_WARNING("Batch lookup request: both ips and macs provided");
                return;
            }

            std::vector<std::string> query_list;
            std::string query_type;

            if (has_ips) {
                // IP batch lookup
                query_type = "IP";
                if (!lookup_handler_) {
                    send_error_response(res, 500, "Internal Server Error", "IP lookup handler not configured");
                    LOG_WARNING("IP batch lookup requested but handler not set");
                    return;
                }

                // Check batch size limit
                size_t batch_size = body["ips"].size();
                if (batch_size > static_cast<size_t>(max_batch_size_)) {
                    nlohmann::json error;
                    error["error"] = "Batch size exceeds maximum limit";
                    error["max_batch_size"] = max_batch_size_;
                    error["requested_size"] = batch_size;
                    send_json_response(res, error, 400);
                    LOG_WARNING("Batch lookup request exceeds size limit: " + std::to_string(batch_size) +
                               " > " + std::to_string(max_batch_size_));
                    return;
                }

                // Extract IP strings
                query_list.reserve(batch_size);
                for (const auto& ip : body["ips"]) {
                    if (ip.is_string()) {
                        query_list.push_back(ip.get<std::string>());
                    }
                }
            } else {
                // MAC batch lookup
                query_type = "MAC";
                if (!mac_lookup_handler_) {
                    send_error_response(res, 500, "Internal Server Error", "MAC lookup handler not configured");
                    LOG_WARNING("MAC batch lookup requested but handler not set");
                    return;
                }

                // Check batch size limit
                size_t batch_size = body["macs"].size();
                if (batch_size > static_cast<size_t>(max_batch_size_)) {
                    nlohmann::json error;
                    error["error"] = "Batch size exceeds maximum limit";
                    error["max_batch_size"] = max_batch_size_;
                    error["requested_size"] = batch_size;
                    send_json_response(res, error, 400);
                    LOG_WARNING("Batch lookup request exceeds size limit: " + std::to_string(batch_size) +
                               " > " + std::to_string(max_batch_size_));
                    return;
                }

                // Extract MAC strings
                query_list.reserve(batch_size);
                for (const auto& mac : body["macs"]) {
                    if (mac.is_string()) {
                        query_list.push_back(mac.get<std::string>());
                    }
                }
            }

            // Parallel lookup using std::async
            std::vector<std::future<LookupResult>> futures;
            futures.reserve(query_list.size());

            for (const auto& query_str : query_list) {
                LOG_DEBUG("Batch lookup for " + query_type + ": " + query_str);
                if (query_type == "IP") {
                    futures.push_back(std::async(std::launch::async, [this, query_str]() {
                        return lookup_handler_(query_str);
                    }));
                } else {
                    futures.push_back(std::async(std::launch::async, [this, query_str]() {
                        return mac_lookup_handler_(query_str);
                    }));
                }
            }

            // Collect results
            nlohmann::json results = nlohmann::json::array();
            for (auto& future : futures) {
                auto result = future.get();
                metrics_->record_request(result.cache_hit, result.latency_ms);
                results.push_back(result.data);
            }

            LOG_INFO("Batch lookup completed for " + std::to_string(results.size()) + " " + query_type + "s");
            res.set_content(results.dump(), "application/json");

        } catch (const nlohmann::json::exception& e) {
            send_error_response(res, 400, "Bad Request", "Invalid JSON: " + std::string(e.what()));
            LOG_WARNING("Invalid JSON in batch lookup request: " + std::string(e.what()));
        } catch (const std::exception& e) {
            send_error_response(res, 500, "Internal Server Error", e.what());
            LOG_ERROR(std::string("Error processing batch lookup: ") + e.what());
        }
    });

    // Note: MAC lookup endpoints have been consolidated into /lookup
    // Use GET /lookup?mac=... for single MAC lookup
    // Use POST /lookup with {"macs": [...]} for batch MAC lookup

    LOG_INFO("HTTP routes configured");
}

bool IPGeoHTTPServer::start(std::atomic<bool>& shutdown_requested) {
    if (!lookup_handler_) {
        LOG_ERROR("Cannot start server: lookup handler not set");
        return false;
    }

    if (!mac_lookup_handler_) {
        LOG_WARNING("MAC lookup handler not set, /mac/lookup endpoints will not work");
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
    std::cout << "  GET  /lookup?ip=<address>    - IP lookup" << std::endl;
    std::cout << "  GET  /lookup?mac=<address>   - MAC lookup" << std::endl;
    std::cout << "  POST /lookup                 - Batch lookup" << std::endl;
    std::cout << "                                Body: {\"ips\": [...]} or {\"macs\": [...]}" << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // Start cleanup thread for rate limiter
    if (enable_rate_limiter_ && rate_limiter_) {
        cleanup_thread_running_.store(true);
        cleanup_thread_ = std::thread([this, &shutdown_requested]() {
            cleanup_thread_func(shutdown_requested);
        });
        LOG_INFO("Rate limiter cleanup thread started");
    }

    // Start server in a separate thread to allow graceful shutdown
    std::atomic<bool> server_running(true);
    std::thread server_thread([this, &server_running]() {
        if (!server_.listen(host_.c_str(), port_)) {
            LOG_ERROR("Failed to start HTTP server");
            server_running.store(false);
        }
    });

    // Wait for server to start or fail
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!server_running.load()) {
        if (server_thread.joinable()) {
            server_thread.join();
        }
        if (cleanup_thread_.joinable()) {
            cleanup_thread_running_.store(false);
            cleanup_thread_.join();
        }
        return false;
    }

    // Wait for shutdown signal from main
    while (!shutdown_requested.load() && server_.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop server and wait for thread to finish
    server_.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }

    // Stop cleanup thread
    if (cleanup_thread_.joinable()) {
        cleanup_thread_running_.store(false);
        cleanup_thread_.join();
        LOG_INFO("Rate limiter cleanup thread stopped");
    }

    return true;
}

void IPGeoHTTPServer::stop() {
    LOG_INFO("Stopping HTTP server");
    server_.stop();
}

void IPGeoHTTPServer::cleanup_thread_func(std::atomic<bool>& shutdown_requested) {
    LOG_INFO("Rate limiter cleanup thread running");

    while (!shutdown_requested.load()) {
        // Sleep for 5 minutes between cleanups
        for (int i = 0; i < 300 && !shutdown_requested.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (shutdown_requested.load()) {
            break;
        }

        // Perform cleanup
        if (rate_limiter_) {
            rate_limiter_->cleanup();

            // Log memory statistics
            auto stats = rate_limiter_->get_memory_stats();
            LOG_INFO("Rate limiter memory stats: " +
                    std::to_string(stats.ip_record_count) + " IP records, " +
                    std::to_string(stats.total_timestamps) + " timestamps, " +
                    std::to_string(stats.estimated_memory_bytes / 1024) + " KB");
        }
    }

    LOG_INFO("Rate limiter cleanup thread exiting");
}

} // namespace ip_server