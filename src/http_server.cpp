#include "http_server.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#include "auth.h"
#include "logger.h"
#include "metrics.h"
#include "password_handler.h"
#include "rate_limiter.h"
#include "validation.h"

namespace ip_server {

IPGeoHTTPServer::IPGeoHTTPServer(std::string host, uint16_t port, int thread_pool_size,
                                 bool enable_rate_limiter, int max_requests_per_minute,
                                 int max_batch_size, bool enable_api_auth,
                                 const std::string& api_keys_file,
                                 const std::string& default_api_key)
    : host_(std::move(host)),
      port_(port),
      thread_pool_size_(thread_pool_size),
      enable_rate_limiter_(enable_rate_limiter),
      max_batch_size_(max_batch_size),
      enable_api_auth_(enable_api_auth),
      metrics_(std::make_unique<Metrics>()),
      password_handler_(metrics_.get()) {

    if (enable_rate_limiter_) {
        rate_limiter_ =
            std::make_unique<RateLimiter>(max_requests_per_minute, std::chrono::seconds(60),
                                          constants::DEFAULT_RATE_LIMITER_MAX_IPS);
        LOG_INFO("Rate limiter enabled: " + std::to_string(max_requests_per_minute)
                 + " requests per minute");
    }

    if (enable_api_auth_) {
        api_auth_ = std::make_unique<APIAuth>(true);

        if (!api_keys_file.empty()) {
            if (api_auth_->load_keys_from_file(api_keys_file)) {
                LOG_INFO("Loaded " + std::to_string(api_auth_->key_count())
                         + " API keys from file");
            }
        }

        if (!default_api_key.empty()) {
            api_auth_->add_key(default_api_key);
            LOG_INFO("Added default API key");
        }

        if (api_auth_->key_count() == 0) {
            LOG_WARNING("API authentication enabled but no API keys configured!");
        }
    }

    LOG_INFO("HTTP server configured for " + host_ + ":" + std::to_string(port_) + " with "
             + std::to_string(thread_pool_size_) + " threads");
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
        return true;
    }

    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) {
        send_error_response(res, 401, "Unauthorized", "Missing API key in Authorization header");
        res.set_header("WWW-Authenticate", "Bearer");
        LOG_WARNING("Unauthorized request: missing API key");
        return false;
    }

    std::string const prefix = "Bearer ";
    if (!auth_header.starts_with(prefix)) {
        send_error_response(res, 401, "Unauthorized",
                            "Invalid Authorization header format. Expected: Bearer <api_key>");
        res.set_header("WWW-Authenticate", "Bearer");
        LOG_WARNING("Unauthorized request: invalid Authorization header format");
        return false;
    }

    std::string const api_key = auth_header.substr(prefix.length());
    if (!api_auth_->is_valid(api_key)) {
        send_error_response(res, 401, "Unauthorized", "Invalid API key");
        LOG_WARNING("Unauthorized request: invalid API key from " + req.remote_addr);
        return false;
    }

    return true;
}

auto IPGeoHTTPServer::prepare_request(const httplib::Request& req, httplib::Response& res)
    -> std::optional<std::string> {
    if (!authenticate_request(req, res)) {
        return std::nullopt;
    }

    auto client_ip = get_real_client_ip(req);
    if (client_ip.empty()) {
        send_error_response(res, 400, "Bad Request", "Unable to determine source IP address");
        LOG_WARNING("Request: unable to determine source IP");
        return std::nullopt;
    }

    if (enable_rate_limiter_ && !rate_limiter_->is_allowed(client_ip)) {
        nlohmann::json error;
        error["error"]     = "Rate limit exceeded";
        error["remaining"] = rate_limiter_->get_remaining(client_ip);
        send_json_response(res, error, 429);
        res.set_header("Retry-After", "60");
        LOG_WARNING("Rate limit exceeded for IP: " + client_ip);
        return std::nullopt;
    }

    return client_ip;
}

void IPGeoHTTPServer::setup_cors() {
    server_.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        return httplib::Server::HandlerResponse::Unhandled;
    });

    server_.Options(".*", [](const httplib::Request&, httplib::Response&) { return; });
}

// ponytail: X-Forwarded-For/X-Real-IP trusted from all clients — no trusted-proxy
// config exists; add one in APIAuth if header spoofing becomes a concern.
std::string IPGeoHTTPServer::get_real_client_ip(const httplib::Request& req) const {
    auto xff = req.get_header_value("X-Forwarded-For");
    if (!xff.empty()) {
        size_t const comma_pos = xff.find(',');
        if (comma_pos != std::string::npos) {
            std::string first_ip = xff.substr(0, comma_pos);
            size_t const start   = first_ip.find_first_not_of(" \t");
            size_t const end     = first_ip.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                return first_ip.substr(start, end - start + 1);
            }
            return first_ip;
        }
        return xff;
    }

    auto xri = req.get_header_value("X-Real-IP");
    if (!xri.empty()) {
        return xri;
    }

    return req.remote_addr;
}

void IPGeoHTTPServer::setup_routes() {
    server_.Get("/", [this](const httplib::Request& req, httplib::Response& res) {
        handle_root(req, res);
    });
    server_.Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handle_health(req, res);
    });
    server_.Get("/lookup", [this](const httplib::Request& req, httplib::Response& res) {
        handle_lookup_get(req, res);
    });
    server_.Post("/lookup", [this](const httplib::Request& req, httplib::Response& res) {
        handle_lookup_post(req, res);
    });
    server_.Get("/password/generate", [this](const httplib::Request& req, httplib::Response& res) {
        password_handler_.handle_get(req, res);
    });
    server_.Post("/password/generate", [this](const httplib::Request& req, httplib::Response& res) {
        password_handler_.handle_post(req, res);
    });
    LOG_INFO("HTTP routes configured");
}

void IPGeoHTTPServer::handle_root(const httplib::Request&, httplib::Response& res) {
    nlohmann::json info;
    info["service"]   = "IP Geolocation & AS Lookup Service";
    info["version"]   = "2.0.0";
    info["endpoints"] = nlohmann::json::array({"/", "/lookup", "/health"});
    info["description"] =
        "Use /lookup with 'ip=' or 'mac=' parameter for single queries, or "
        "'ips'/'macs' array for batch queries";
    res.set_content(info.dump(), "application/json");
}

void IPGeoHTTPServer::handle_health(const httplib::Request&, httplib::Response& res) {
    auto stats = metrics_->get_stats();

    nlohmann::json health;
    health["status"]    = "ok";
    health["timestamp"] = std::time(nullptr);

    health["databases"] = {{"city", {{"open", stats.city_db_open_}}},
                           {"asn", {{"open", stats.asn_db_open_}}},
                           {"oui", {{"open", stats.oui_db_open_}}}};

    health["metrics"] = {{"total_requests", stats.total_requests_},
                         {"qps", round(stats.current_qps_ * 100) / 100},
                         {"avg_latency_ms", round(stats.avg_latency_ms_ * 1000) / 1000},
                         {"p50_latency_ms", round(stats.p50_latency_ms_ * 1000) / 1000},
                         {"p95_latency_ms", round(stats.p95_latency_ms_ * 1000) / 1000},
                         {"p99_latency_ms", round(stats.p99_latency_ms_ * 1000) / 1000}};

    health["cache"] = {{"hits", stats.cache_hits_},
                       {"misses", stats.cache_misses_},
                       {"hit_rate_percent", round(stats.cache_hit_rate_ * 100) / 100}};

    health["errors"] = {{"total", stats.total_errors_},
                        {"rate_percent", round(stats.error_rate_ * 100) / 100}};

    health["system"] = {{"uptime_seconds", stats.uptime_seconds_}};

    res.set_content(health.dump(), "application/json");
}

void IPGeoHTTPServer::handle_lookup_get(const httplib::Request& req, httplib::Response& res) {
    auto client_ip = prepare_request(req, res);
    if (!client_ip) return;

    auto mac_param = req.get_param_value("mac");
    auto ip_param  = req.get_param_value("ip");

    if (mac_param.empty() && ip_param.empty()) {
        ip_param = *client_ip;
        LOG_DEBUG("Lookup request using source IP: " + ip_param);
    }

    if (!mac_param.empty() && !ip_param.empty()) {
        send_error_response(res, 400, "Bad Request",
                            "Cannot specify both 'ip' and 'mac' parameters simultaneously");
        LOG_WARNING("Lookup request: both ip and mac parameters provided");
        return;
    }

    try {
        if (!mac_param.empty()) {
            if (!isValidMacFormat(mac_param)) {
                send_error_response(res, 400, "Bad Request", "Invalid MAC address format");
                return;
            }
            if (!mac_lookup_handler_) {
                send_error_response(res, 500, "Internal Server Error",
                                    "MAC lookup handler not configured");
                LOG_WARNING("MAC lookup requested but handler not set");
                return;
            }
            LOG_DEBUG("MAC lookup request for: " + mac_param);
            auto result = mac_lookup_handler_(mac_param);
            metrics_->record_request(result.cache_hit_, result.latency_ms_);
            res.set_content(result.data_.dump(), "application/json");
        } else {
            if (!isValidIpFormat(ip_param)) {
                send_error_response(res, 400, "Bad Request", "Invalid IP address format");
                return;
            }
            if (!lookup_handler_) {
                send_error_response(res, 500, "Internal Server Error",
                                    "IP lookup handler not configured");
                LOG_WARNING("IP lookup requested but handler not set");
                return;
            }
            LOG_DEBUG("IP lookup request for: " + ip_param);
            auto result = lookup_handler_(ip_param);
            metrics_->record_request(result.cache_hit_, result.latency_ms_);
            res.set_content(result.data_.dump(), "application/json");
        }
    } catch (const std::exception& e) {
        res.status = 500;
        nlohmann::json error;
        error["error"] = e.what();
        res.set_content(error.dump(), "application/json");
        metrics_->record_error();
        std::string const query_param = mac_param.empty() ? ip_param : mac_param;
        LOG_ERROR(std::string("Error processing lookup for ") + query_param + ": " + e.what());
    }
}

void IPGeoHTTPServer::handle_lookup_post(const httplib::Request& req, httplib::Response& res) {
    auto client_ip = prepare_request(req, res);
    if (!client_ip) return;

    try {
        auto body = nlohmann::json::parse(req.body);

        bool const has_ips  = body.contains("ips") && body["ips"].is_array();
        bool const has_macs = body.contains("macs") && body["macs"].is_array();

        if (!has_ips && !has_macs) {
            send_error_response(res, 400, "Bad Request",
                                "Missing or invalid 'ips' or 'macs' array in request body");
            LOG_WARNING("Batch lookup request missing 'ips' or 'macs' array");
            return;
        }

        if (has_ips && has_macs) {
            send_error_response(res, 400, "Bad Request",
                                "Cannot specify both 'ips' and 'macs' in the same request");
            LOG_WARNING("Batch lookup request: both ips and macs provided");
            return;
        }

        std::vector<std::string> query_list;
        std::string query_type;
        LookupHandler handler;

        auto process_batch = [&](LookupHandler h, const char* type, const char* json_key,
                                 bool (*validator)(const std::string&)) -> bool {
            query_type = type;
            if (!h) {
                send_error_response(res, 500, "Internal Server Error",
                                    std::string(type) + " lookup handler not configured");
                return false;
            }
            handler = h;

            auto const& items       = body[json_key];
            size_t const batch_size = items.size();
            if (std::cmp_greater(batch_size, max_batch_size_)) {
                nlohmann::json error;
                error["error"]          = "Batch size exceeds maximum limit";
                error["max_batch_size"] = max_batch_size_;
                error["requested_size"] = batch_size;
                send_json_response(res, error, 400);
                return false;
            }

            query_list.reserve(batch_size);
            for (const auto& item : items) {
                if (item.is_string()) {
                    std::string s = item.get<std::string>();
                    if (validator(s)) {
                        query_list.push_back(std::move(s));
                    }
                }
            }
            return true;
        };

        if (has_ips) {
            if (!process_batch(lookup_handler_, "IP", "ips", isValidIpFormat)) return;
        } else {
            if (!process_batch(mac_lookup_handler_, "MAC", "macs", isValidMacFormat)) return;
        }

        nlohmann::json results = nlohmann::json::array();
        results.get_ptr<nlohmann::json::array_t*>()->reserve(query_list.size());

        if (query_list.size() <= 10) {
            for (const auto& query_str : query_list) {
                auto result = handler(query_str);
                metrics_->record_request(result.cache_hit_, result.latency_ms_);
                results.push_back(std::move(result.data_));
            }
        } else {
            std::vector<std::future<LookupResult>> futures;
            futures.reserve(query_list.size());

            for (const auto& query_str : query_list) {
                futures.push_back(std::async(std::launch::async, [handler, query_str]() {
                    return handler(query_str);
                }));
            }

            for (auto& future : futures) {
                auto result = future.get();
                metrics_->record_request(result.cache_hit_, result.latency_ms_);
                results.push_back(std::move(result.data_));
            }
        }

        LOG_INFO("Batch lookup completed for " + std::to_string(results.size()) + " " + query_type
                 + "s");
        res.set_content(results.dump(), "application/json");

    } catch (const nlohmann::json::exception& e) {
        send_error_response(res, 400, "Bad Request", "Invalid JSON: " + std::string(e.what()));
        LOG_WARNING("Invalid JSON in batch lookup request: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(res, 500, "Internal Server Error", e.what());
        metrics_->record_error();
        LOG_ERROR(std::string("Error processing batch lookup: ") + e.what());
    }
}

bool IPGeoHTTPServer::start(std::atomic<bool>& shutdown_requested) {
    if (!lookup_handler_) {
        LOG_ERROR("Cannot start server: lookup handler not set");
        return false;
    }

    if (!mac_lookup_handler_) {
        LOG_WARNING("MAC lookup handler not set, /lookup?mac= endpoints will not work");
    }

    setup_cors();
    setup_routes();
    server_.new_task_queue = [this] { return new httplib::ThreadPool(thread_pool_size_); };
    log_startup_banner();

    if (enable_rate_limiter_ && rate_limiter_) {
        cleanup_thread_ = std::jthread(
            [this, &shutdown_requested]() { cleanup_thread_func(shutdown_requested); });
    }

    bool const result = start_server_and_wait(shutdown_requested);

    if (cleanup_thread_.joinable()) {
        cleanup_thread_.request_stop();
    }

    return result;
}

void IPGeoHTTPServer::log_startup_banner() const {
    LOG_INFO("Starting HTTP server on " + host_ + ":" + std::to_string(port_));
    LOG_INFO("========================================");
    LOG_INFO("  IP Geolocation & AS Lookup Service");
    LOG_INFO("========================================");
    LOG_INFO("Server: http://" + host_ + ":" + std::to_string(port_));
    LOG_INFO("Thread Pool: " + std::to_string(thread_pool_size_) + " threads");
    LOG_INFO("");
    LOG_INFO("API Endpoints:");
    LOG_INFO("  GET  /                       - Service info");
    LOG_INFO("  GET  /health                 - Health check");
    LOG_INFO("  GET  /lookup?ip=<address>    - IP lookup");
    LOG_INFO("  GET  /lookup?mac=<address>   - MAC lookup");
    LOG_INFO("  POST /lookup                 - Batch lookup");
    LOG_INFO("  GET  /password/generate      - Generate password");
    LOG_INFO("  POST /password/generate      - Batch generate passwords");
    LOG_INFO("");
    LOG_INFO("Press Ctrl+C to stop the server");
    LOG_INFO("========================================");
}

bool IPGeoHTTPServer::start_server_and_wait(std::atomic<bool>& shutdown_requested) {
    std::atomic<bool> server_running(true);
    std::mutex server_mutex;
    std::condition_variable server_cv;
    std::thread server_thread([this, &server_running, &server_cv]() {
        server_.listen(host_, port_);
        server_running.store(false);
        server_cv.notify_all();
    });

    {
        std::unique_lock lock(server_mutex);
        if (!server_cv.wait_for(lock, std::chrono::milliseconds(500),
                                [&] { return !server_running.load(); })) {
            LOG_DEBUG("Server started successfully");
        } else {
            if (server_thread.joinable()) {
                server_thread.join();
            }
            return false;
        }
    }

    {
        std::unique_lock lock(server_mutex);
        while (!shutdown_requested.load() && server_running.load()) {
            server_cv.wait_for(lock, std::chrono::milliseconds(100));
        }
    }

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

void IPGeoHTTPServer::cleanup_thread_func(std::atomic<bool>& shutdown_requested) {
    LOG_INFO("Rate limiter cleanup thread running");

    while (!shutdown_requested.load()) {
        for (int i = 0; i < constants::CLEANUP_INTERVAL_SECONDS && !shutdown_requested.load();
             ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (shutdown_requested.load()) {
            break;
        }

        if (rate_limiter_) {
            rate_limiter_->cleanup();

            auto stats = rate_limiter_->get_memory_stats();
            LOG_INFO("Rate limiter memory stats: " + std::to_string(stats.ip_record_count_)
                     + " IP records, " + std::to_string(stats.total_timestamps_) + " timestamps");
        }
    }

    LOG_INFO("Rate limiter cleanup thread exiting");
}

}  // namespace ip_server
