#include "config.h"
#include "database.h"
#include "http_server.h"
#include "logger.h"
#include "xdg.h"
#include <iostream>
#include <csignal>
#include <memory>

namespace ip_server {

class Application {
public:
    Application(const ServerConfig& config)
        : config_(config),
          geo_service_(config.city_db_path, config.asn_db_path),
          http_server_(config.host, config.port) {

        http_server_.set_lookup_handler([this](const std::string& ip) {
            return geo_service_.lookup(ip);
        });
    }

    bool run() {
        LOG_INFO("Application starting...");

        // Setup signal handlers for graceful shutdown
        std::signal(SIGINT, [](int) {
            LOG_INFO("Received SIGINT, shutting down...");
            std::exit(0);
        });

        std::signal(SIGTERM, [](int) {
            LOG_INFO("Received SIGTERM, shutting down...");
            std::exit(0);
        });

        return http_server_.start();
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