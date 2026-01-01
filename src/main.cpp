#include "config.h"
#include "database.h"
#include "http_server.h"
#include "logger.h"
#include "xdg.h"
#include <iostream>
#include <csignal>
#include <memory>
#include <atomic>

namespace ip_server {

class Application {
public:
    Application(const ServerConfig& config)
        : config_(config),
          geo_service_(config.city_db_path, config.asn_db_path, config.cache_size),
          http_server_(config.host, config.port, config.thread_pool_size,
                      config.enable_rate_limiter, config.max_requests_per_minute,
                      config.max_batch_size, config.enable_api_auth,
                      config.api_keys_file, config.default_api_key) {

        http_server_.set_lookup_handler([this](const std::string& ip) {
            return geo_service_.lookup(ip);
        });
    }

    bool run() {
        LOG_INFO("Application starting...");

        // Setup atomic flag for graceful shutdown
        static std::atomic<bool> shutdown_requested(false);

        // Setup signal handlers for graceful shutdown
        std::signal(SIGINT, [](int) {
            if (!shutdown_requested.exchange(true)) {
                LOG_INFO("Received SIGINT, shutting down gracefully...");
            }
        });

        std::signal(SIGTERM, [](int) {
            if (!shutdown_requested.exchange(true)) {
                LOG_INFO("Received SIGTERM, shutting down gracefully...");
            }
        });

        // Start server and wait for shutdown signal
        bool result = http_server_.start();

        // Perform graceful shutdown
        if (shutdown_requested.load()) {
            LOG_INFO("Performing graceful shutdown...");
            http_server_.stop();
            LOG_INFO("Application shutdown complete");
        }

        return result;
    }

private:
    ServerConfig config_;
    IPGeoService geo_service_;
    IPGeoHTTPServer http_server_;
};

} // namespace ip_server

int main(int argc, char* argv[]) {
    try {
        using namespace ip_server;

        // Parse configuration
        auto config = ConfigParser::parse(argc, argv);

        // Ensure XDG directories exist
        if (config.use_xdg) {
            XDGPaths::instance().ensure_directories();

            // Create default config file if it doesn't exist
            if (!config.config_file.empty() && !std::filesystem::exists(config.config_file)) {
                LOG_INFO("Creating default config file: " + config.config_file.string());
                ConfigParser::save_to_file(config, config.config_file);
            }
        }

        // Create and run application
        Application app(config);
        if (!app.run()) {
            LOG_ERROR("Application failed to start");
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        std::cerr << "\nPlease ensure you have valid MaxMind database files." << std::endl;
        std::cerr << "Download from: https://dev.maxmind.com/geoip/geolite2-free-geolocation-data" << std::endl;
        return 1;
    }

    return 0;
}