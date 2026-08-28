#include <atomic>
#include <csignal>
#include <cstring>
#include <string>

#include "config.h"
#include "http_server.h"
#include "logger.h"
#include "service/ip_geo_service.h"
#include "service/mac_lookup_service.h"
#include "paths.h"

namespace ip_server {

namespace {

std::atomic<bool> g_shutdown_requested{false};

extern "C" void handle_signal(int sig) {
    if (!g_shutdown_requested.exchange(true)) {
        const char* name = (sig == SIGINT) ? "SIGINT" : "SIGTERM";
        signal_safe_log(name);
    }
}

bool run_application(const ServerConfig& config) {
    IPGeoService geo_service(config.city_db_path_, config.asn_db_path_);
    MACLookupService mac_service(config.oui_db_path_);
    IPGeoHTTPServer http_server(config);

    http_server.set_lookup_handler(
        [&geo_service](const std::string& ip) { return geo_service.lookup(ip); });

    http_server.set_mac_lookup_handler(
        [&mac_service](const std::string& mac) { return mac_service.lookup(mac); });

    http_server.set_cache_stats_handler([&geo_service, &mac_service] {
        CacheStats stats = geo_service.cache_stats();
        stats += mac_service.cache_stats();
        return stats;
    });

    LOG_INFO("Application starting...");

    g_shutdown_requested.store(false);

    struct sigaction sa{};
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // Use the global shutdown flag so the signal handler (C function pointer)
    // can communicate with the server's wait loop.
    bool const result = http_server.start(g_shutdown_requested);

    if (g_shutdown_requested.load()) {
        LOG_INFO("Performing graceful shutdown...");
        http_server.stop();
        LOG_INFO("Application shutdown complete");
    }

    return result;
}

}  // namespace

}  // namespace ip_server

int main(int argc, char* argv[]) {
    try {
        using namespace ip_server;

        auto config = ConfigParser::parse(argc, argv);

        init_logging({.enable_file_logging_ = config.enable_file_logging_,
                      .log_file_path_ = config.log_file_path_,
                      .rotation_type_ = config.log_rotation_type_,
                      .max_file_size_ = config.log_max_file_size_,
                      .max_backup_files_ = config.log_max_backup_files_,
                      .enable_stdout_ = config.log_enable_stdout_});

        set_log_level(config.log_level_);

        paths::ensure_directories();

        if (!config.config_file_.empty() && !std::filesystem::exists(config.config_file_)) {
            LOG_INFO("Creating default config file: " + config.config_file_.string());
            ConfigParser::save_to_file(config, config.config_file_);
        }

        if (!run_application(config)) {
            LOG_ERROR("Application failed to start");
            return 1;
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: " + std::string(e.what()));
        LOG_ERROR("");
        LOG_ERROR("Please ensure you have valid MaxMind database files.");
        LOG_ERROR(
            "Download from: "
            "https://dev.maxmind.com/geoip/geolite2-free-geolocation-data");
        return 1;
    }

    return 0;
}
