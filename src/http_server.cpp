#include "http_server.h"

#include <atomic>
#include <chrono>
#include <future>
#include <regex>
#include <stdexcept>
#include <vector>

#include "auth.h"
#include "logger.h"
#include "metrics.h"
#include "password_generator.h"
#include "rate_limiter.h"

namespace ip_server {

namespace {

bool is_valid_ip_format(const std::string& ip) {
    static const std::regex ipv4_pattern(
        R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
    static const std::regex ipv6_pattern(
        R"(^([0-9a-fA-F]{0,4}:){2,7}[0-9a-fA-F]{0,4}$|^::$|^::1$|^([0-9a-fA-F]{0,4}:)+:.*$)");

    if (std::regex_match(ip, ipv4_pattern)) {
        return true;
    }
    return std::regex_match(ip, ipv6_pattern);
}

bool is_valid_mac_format(const std::string& mac) {
    static const std::regex mac_pattern(
        R"(^([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}$|^[0-9A-Fa-f]{12}$)");
    return std::regex_match(mac, mac_pattern);
}

}  // namespace

IPGeoHTTPServer::IPGeoHTTPServer(const std::string& host, uint16_t port, int thread_pool_size,
                                 bool enable_rate_limiter, int max_requests_per_minute,
                                 int max_batch_size, bool enable_api_auth,
                                 const std::string& api_keys_file,
                                 const std::string& default_api_key,
                                 const std::vector<std::string>& trusted_proxies)
    : host_(host),
      port_(port),
      thread_pool_size_(thread_pool_size),
      enable_rate_limiter_(enable_rate_limiter),
      max_batch_size_(max_batch_size),
      enable_api_auth_(enable_api_auth),
      trusted_proxies_(trusted_proxies) {
    metrics_ = std::make_unique<Metrics>();

    password_generator_ = std::make_unique<PasswordGenerator>();

    if (enable_rate_limiter_) {
        rate_limiter_ = std::make_unique<RateLimiter>(
            max_requests_per_minute, std::chrono::seconds(60),
            constants::DEFAULT_RATE_LIMITER_MAX_IPS);
        LOG_INFO("Rate limiter enabled: " + std::to_string(max_requests_per_minute)
                 + " requests per minute");
    }

    if (enable_api_auth_) {
        api_auth_ = std::make_unique<APIAuth>(true);

        if (!trusted_proxies_.empty()) {
            api_auth_->set_trusted_proxies(trusted_proxies_);
            LOG_INFO("Trusted proxies configured: " + std::to_string(trusted_proxies_.size())
                     + " addresses");
        }

        if (!api_keys_file.empty()) {
            if (api_auth_->load_keys_from_file(api_keys_file)) {
                LOG_INFO("Loaded " + std::to_string(api_auth_->key_count()) + " API keys from file");
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
    if (auth_header.substr(0, prefix.length()) != prefix) {
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

void IPGeoHTTPServer::send_error_response(httplib::Response& res, int status,
                                          const std::string& error, const std::string& message) {
    nlohmann::json error_json;
    error_json["error"] = error;
    error_json["message"] = message;
    res.status = status;
    res.set_content(error_json.dump(), "application/json");
}

void IPGeoHTTPServer::send_json_response(httplib::Response& res, const nlohmann::json& data,
                                         int status) {
    res.status = status;
    res.set_content(data.dump(), "application/json");
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

bool IPGeoHTTPServer::is_trusted_proxy(const std::string& ip) const {
    if (trusted_proxies_.empty()) {
        return true;
    }
    return std::find(trusted_proxies_.begin(), trusted_proxies_.end(), ip) != trusted_proxies_.end();
}

std::string IPGeoHTTPServer::get_real_client_ip(const httplib::Request& req) const {
    if (is_trusted_proxy(req.remote_addr)) {
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
    }

    return req.remote_addr;
}

void IPGeoHTTPServer::setup_routes() {
    server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json info;
        info["service"] = "IP Geolocation & AS Lookup Service";
        info["version"] = "2.0.0";
        info["endpoints"] = nlohmann::json::array({"/", "/lookup", "/health"});
        info["description"] =
            "Use /lookup with 'ip=' or 'mac=' parameter for single queries, or "
            "'ips'/'macs' array for batch queries";
        res.set_content(info.dump(), "application/json");
    });

    server_.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        auto stats = metrics_->get_stats();

        nlohmann::json health;
        health["status"] = "ok";
        health["timestamp"] = std::time(nullptr);

        health["databases"] = {{"city", {{"open", stats.city_db_open}}},
                               {"asn", {{"open", stats.asn_db_open}}},
                               {"oui", {{"open", stats.oui_db_open}}}};

        health["metrics"] = {{"total_requests", stats.total_requests},
                             {"qps", round(stats.current_qps * 100) / 100},
                             {"avg_latency_ms", round(stats.avg_latency_ms * 1000) / 1000},
                             {"p50_latency_ms", round(stats.p50_latency_ms * 1000) / 1000},
                             {"p95_latency_ms", round(stats.p95_latency_ms * 1000) / 1000},
                             {"p99_latency_ms", round(stats.p99_latency_ms * 1000) / 1000}};

        health["cache"] = {{"hits", stats.cache_hits},
                           {"misses", stats.cache_misses},
                           {"hit_rate_percent", round(stats.cache_hit_rate * 100) / 100},
                           {"evictions", stats.cache_evictions}};

        health["errors"] = {{"total", stats.total_errors},
                            {"rate_percent", round(stats.error_rate * 100) / 100}};

        health["system"] = {{"memory_usage_mb", stats.memory_usage_mb},
                            {"uptime_seconds", stats.uptime_seconds}};

        res.set_content(health.dump(), "application/json");
    });

    server_.Get("/lookup", [this](const httplib::Request& req, httplib::Response& res) {
        if (!authenticate_request(req, res)) {
            return;
        }

        auto client_ip = get_real_client_ip(req);
        if (client_ip.empty()) {
            send_error_response(res, 400, "Bad Request", "Unable to determine source IP address");
            LOG_WARNING("Lookup request: unable to determine source IP");
            return;
        }

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

        if (mac_param.empty() && ip_param.empty()) {
            ip_param = client_ip;
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
                if (!is_valid_mac_format(mac_param)) {
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

                metrics_->record_request(result.cache_hit, result.latency_ms);

                res.set_content(result.data.dump(), "application/json");
            } else {
                if (!is_valid_ip_format(ip_param)) {
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

                metrics_->record_request(result.cache_hit, result.latency_ms);

                res.set_content(result.data.dump(), "application/json");
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
    });

    server_.Post("/lookup", [this](const httplib::Request& req, httplib::Response& res) {
        if (!authenticate_request(req, res)) {
            return;
        }

        auto client_ip = get_real_client_ip(req);
        if (client_ip.empty()) {
            send_error_response(res, 400, "Bad Request", "Unable to determine source IP address");
            LOG_WARNING("Batch lookup request: unable to determine source IP");
            return;
        }

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

            if (has_ips) {
                query_type = "IP";
                if (!lookup_handler_) {
                    send_error_response(res, 500, "Internal Server Error",
                                        "IP lookup handler not configured");
                    return;
                }
                handler = lookup_handler_;

                size_t const batch_size = body["ips"].size();
                if (batch_size > static_cast<size_t>(max_batch_size_)) {
                    nlohmann::json error;
                    error["error"] = "Batch size exceeds maximum limit";
                    error["max_batch_size"] = max_batch_size_;
                    error["requested_size"] = batch_size;
                    send_json_response(res, error, 400);
                    return;
                }

                query_list.reserve(batch_size);
                for (const auto& ip : body["ips"]) {
                    if (ip.is_string()) {
                        std::string ip_str = ip.get<std::string>();
                        if (is_valid_ip_format(ip_str)) {
                            query_list.push_back(std::move(ip_str));
                        }
                    }
                }
            } else {
                query_type = "MAC";
                if (!mac_lookup_handler_) {
                    send_error_response(res, 500, "Internal Server Error",
                                        "MAC lookup handler not configured");
                    return;
                }
                handler = mac_lookup_handler_;

                size_t const batch_size = body["macs"].size();
                if (batch_size > static_cast<size_t>(max_batch_size_)) {
                    nlohmann::json error;
                    error["error"] = "Batch size exceeds maximum limit";
                    error["max_batch_size"] = max_batch_size_;
                    error["requested_size"] = batch_size;
                    send_json_response(res, error, 400);
                    return;
                }

                query_list.reserve(batch_size);
                for (const auto& mac : body["macs"]) {
                    if (mac.is_string()) {
                        std::string mac_str = mac.get<std::string>();
                        if (is_valid_mac_format(mac_str)) {
                            query_list.push_back(std::move(mac_str));
                        }
                    }
                }
            }

            nlohmann::json results = nlohmann::json::array();

            if (query_list.size() <= 10) {
                for (const auto& query_str : query_list) {
                    auto result = handler(query_str);
                    metrics_->record_request(result.cache_hit, result.latency_ms);
                    results.push_back(std::move(result.data));
                }
            } else {
                std::vector<std::future<LookupResult>> futures;
                futures.reserve(query_list.size());

                for (const auto& query_str : query_list) {
                    futures.push_back(std::async(std::launch::async, [&handler, &query_str]() {
                        return handler(query_str);
                    }));
                }

                for (auto& future : futures) {
                    auto result = future.get();
                    metrics_->record_request(result.cache_hit, result.latency_ms);
                    results.push_back(std::move(result.data));
                }
            }

            LOG_INFO("Batch lookup completed for " + std::to_string(results.size()) + " "
                     + query_type + "s");
            res.set_content(results.dump(), "application/json");

        } catch (const nlohmann::json::exception& e) {
            send_error_response(res, 400, "Bad Request", "Invalid JSON: " + std::string(e.what()));
            LOG_WARNING("Invalid JSON in batch lookup request: " + std::string(e.what()));
        } catch (const std::exception& e) {
            send_error_response(res, 500, "Internal Server Error", e.what());
            metrics_->record_error();
            LOG_ERROR(std::string("Error processing batch lookup: ") + e.what());
        }
    });

    server_.Get("/password/generate", [this](const httplib::Request& req, httplib::Response& res) {
        if (!authenticate_request(req, res)) {
            return;
        }

        auto client_ip = get_real_client_ip(req);
        if (client_ip.empty()) {
            send_error_response(res, 400, "Bad Request", "Unable to determine source IP address");
            return;
        }

        if (enable_rate_limiter_ && !rate_limiter_->is_allowed(client_ip)) {
            nlohmann::json error;
            error["error"] = "Rate limit exceeded";
            error["remaining"] = rate_limiter_->get_remaining(client_ip);
            send_json_response(res, error, 429);
            res.set_header("Retry-After", "60");
            return;
        }

        try {
            PasswordConfig config;

            auto length_param = req.get_param_value("length");
            if (!length_param.empty()) {
                config.length = std::stoi(length_param);
            }

            auto uppercase_param = req.get_param_value("uppercase");
            if (!uppercase_param.empty()) {
                config.uppercase = (uppercase_param == "true" || uppercase_param == "1");
            }

            auto lowercase_param = req.get_param_value("lowercase");
            if (!lowercase_param.empty()) {
                config.lowercase = (lowercase_param == "true" || lowercase_param == "1");
            }

            auto digits_param = req.get_param_value("digits");
            if (!digits_param.empty()) {
                config.digits = (digits_param == "true" || digits_param == "1");
            }

            auto symbols_param = req.get_param_value("symbols");
            if (!symbols_param.empty()) {
                config.symbols = (symbols_param == "true" || symbols_param == "1");
            }

            auto exclude_similar_param = req.get_param_value("exclude_similar");
            if (!exclude_similar_param.empty()) {
                config.exclude_similar = (exclude_similar_param == "true" || exclude_similar_param == "1");
            }

            std::string error_message;
            if (!PasswordGenerator::validate_config(config, error_message)) {
                send_error_response(res, 400, "Bad Request", error_message);
                return;
            }

            auto start_time = std::chrono::high_resolution_clock::now();
            auto result = password_generator_->generate(config);
            auto end_time = std::chrono::high_resolution_clock::now();
            auto latency_ms =
                std::chrono::duration<double, std::milli>(end_time - start_time).count();

            metrics_->record_request(false, latency_ms);

            nlohmann::json response;
            response["password"] = result.password;
            response["length"] = result.length;
            response["entropy"] = result.entropy;
            response["strength"] = result.strength;

            send_json_response(res, response, 200);

        } catch (const std::invalid_argument& e) {
            send_error_response(res, 400, "Bad Request",
                                "Invalid parameter value: " + std::string(e.what()));
        } catch (const std::exception& e) {
            send_error_response(res, 500, "Internal Server Error", e.what());
            metrics_->record_error();
            LOG_ERROR("Error generating password: " + std::string(e.what()));
        }
    });

    server_.Post("/password/generate", [this](const httplib::Request& req, httplib::Response& res) {
        if (!authenticate_request(req, res)) {
            return;
        }

        auto client_ip = get_real_client_ip(req);
        if (client_ip.empty()) {
            send_error_response(res, 400, "Bad Request", "Unable to determine source IP address");
            return;
        }

        if (enable_rate_limiter_ && !rate_limiter_->is_allowed(client_ip)) {
            nlohmann::json error;
            error["error"] = "Rate limit exceeded";
            error["remaining"] = rate_limiter_->get_remaining(client_ip);
            send_json_response(res, error, 429);
            res.set_header("Retry-After", "60");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);

            PasswordConfig config;

            if (body.contains("length") && body["length"].is_number_integer()) {
                config.length = body["length"].get<int>();
            }

            if (body.contains("uppercase") && body["uppercase"].is_boolean()) {
                config.uppercase = body["uppercase"].get<bool>();
            }

            if (body.contains("lowercase") && body["lowercase"].is_boolean()) {
                config.lowercase = body["lowercase"].get<bool>();
            }

            if (body.contains("digits") && body["digits"].is_boolean()) {
                config.digits = body["digits"].get<bool>();
            }

            if (body.contains("symbols") && body["symbols"].is_boolean()) {
                config.symbols = body["symbols"].get<bool>();
            }

            if (body.contains("exclude_similar") && body["exclude_similar"].is_boolean()) {
                config.exclude_similar = body["exclude_similar"].get<bool>();
            }

            int count = 1;
            if (body.contains("count") && body["count"].is_number_integer()) {
                count = body["count"].get<int>();
            }

            if (count < 1) {
                send_error_response(res, 400, "Bad Request", "Count must be at least 1");
                return;
            }

            if (count > constants::MAX_PASSWORD_BATCH) {
                send_error_response(res, 400, "Bad Request",
                                    "Count cannot exceed " + std::to_string(constants::MAX_PASSWORD_BATCH));
                return;
            }

            std::string error_message;
            if (!PasswordGenerator::validate_config(config, error_message)) {
                send_error_response(res, 400, "Bad Request", error_message);
                return;
            }

            auto start_time = std::chrono::high_resolution_clock::now();
            auto results = password_generator_->generate_batch(config, count);
            auto end_time = std::chrono::high_resolution_clock::now();
            auto latency_ms =
                std::chrono::duration<double, std::milli>(end_time - start_time).count();

            metrics_->record_request(false, latency_ms);

            nlohmann::json response;
            response["count"] = static_cast<int>(results.size());
            response["passwords"] = nlohmann::json::array();

            for (const auto& result : results) {
                nlohmann::json password_json;
                password_json["password"] = result.password;
                password_json["length"] = result.length;
                password_json["entropy"] = result.entropy;
                password_json["strength"] = result.strength;
                response["passwords"].push_back(password_json);
            }

            send_json_response(res, response, 200);

        } catch (const nlohmann::json::exception& e) {
            send_error_response(res, 400, "Bad Request", "Invalid JSON: " + std::string(e.what()));
        } catch (const std::exception& e) {
            send_error_response(res, 500, "Internal Server Error", e.what());
            metrics_->record_error();
            LOG_ERROR("Error generating batch passwords: " + std::string(e.what()));
        }
    });

    LOG_INFO("HTTP routes configured");
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

    if (enable_rate_limiter_ && rate_limiter_) {
        cleanup_thread_running_.store(true);
        cleanup_thread_ = std::jthread(
            [this, &shutdown_requested]() { cleanup_thread_func(shutdown_requested); });
    }

    std::atomic<bool> server_running(true);
    std::thread server_thread([this, &server_running]() {
        if (!server_.listen(host_, port_)) {
            LOG_ERROR("Failed to start HTTP server");
            server_running.store(false);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!server_running.load()) {
        if (server_thread.joinable()) {
            server_thread.join();
        }
        return false;
    }

    auto start_time = std::chrono::steady_clock::now();
    while (!shutdown_requested.load() && server_.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time);
        if (elapsed.count() > constants::SHUTDOWN_TIMEOUT_SECONDS && !server_.is_running()) {
            LOG_WARNING("Server shutdown timeout reached");
            break;
        }
    }

    server_.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }

    if (cleanup_thread_.joinable()) {
        cleanup_thread_running_.store(false);
        cleanup_thread_.request_stop();
    }

    return true;
}

void IPGeoHTTPServer::stop() {
    LOG_INFO("Stopping HTTP server");
    server_.stop();
}

bool IPGeoHTTPServer::is_running() const {
    return server_.is_running();
}

void IPGeoHTTPServer::cleanup_thread_func(std::atomic<bool>& shutdown_requested) {
    LOG_INFO("Rate limiter cleanup thread running");

    while (!shutdown_requested.load()) {
        for (int i = 0; i < constants::CLEANUP_INTERVAL_SECONDS && !shutdown_requested.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (shutdown_requested.load()) {
            break;
        }

        if (rate_limiter_) {
            rate_limiter_->cleanup();

            auto stats = rate_limiter_->get_memory_stats();
            LOG_INFO("Rate limiter memory stats: " + std::to_string(stats.ip_record_count)
                     + " IP records, " + std::to_string(stats.total_timestamps) + " timestamps, "
                     + std::to_string(stats.estimated_memory_bytes / 1024) + " KB");
        }
    }

    LOG_INFO("Rate limiter cleanup thread exiting");
}

}  // namespace ip_server
