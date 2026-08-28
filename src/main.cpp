#include <atomic>
#include <csignal>
#include <cstring>
#include <string>

#include "config.h"
#include "http_server.h"
#include "logger.h"
#include "metrics.h"
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

}  // namespace

class Application {
   public:
    Application(const ServerConfig& config)
        : geo_service_(config.city_db_path_, config.asn_db_path_, config.cache_size_),
          mac_service_(config.oui_db_path_, config.cache_size_),
          http_server_(config) {
        http_server_.set_lookup_handler(
            [this](const std::string& ip) { return geo_service_.lookup(ip); });

        http_server_.set_mac_lookup_handler(
            [this](const std::string& mac) { return mac_service_.lookup(mac); });

        http_server_.set_cache_stats_handler([this] {
            CacheStats stats = geo_service_.cache_stats();
            stats += mac_service_.cache_stats();
            return stats;
        });
    }

    bool run() {
        LOG_INFO("Application starting...");

        g_shutdown_requested.store(false);

        struct sigaction sa{};
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_handler = &handle_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);

        if (auto* metrics = http_server_.get_metrics()) {
            metrics->set_db_status(geo_service_.is_city_db_open(), geo_service_.is_asn_db_open(),
                                   mac_service_.is_oui_db_open());
        }

        // Use the global shutdown flag so the signal handler (C function pointer)
        // can communicate with the server's wait loop.
        bool const result = http_server_.start(g_shutdown_requested);

        if (g_shutdown_requested.load()) {
            LOG_INFO("Performing graceful shutdown...");
            http_server_.stop();
            LOG_INFO("Application shutdown complete");
        }

        return result;
    }

   private:
    IPGeoService geo_service_;
    MACLookupService mac_service_;
    IPGeoHTTPServer http_server_;
};

}  // namespace ip_server

int main(int argc, char* argv[]) {
    try {
        using namespace ip_server;

        auto config = ConfigParser::parse(argc, argv);

        {
            LogConfig log_config;
            log_config.enable_file_logging_ = config.enable_file_logging_;
            log_config.log_file_path_       = config.log_file_path_;
            log_config.enable_stdout_       = config.log_enable_stdout_;
            log_config.rotation_type_       = config.log_rotation_type_;
            log_config.max_file_size_       = config.log_max_file_size_;
            log_config.max_backup_files_    = config.log_max_backup_files_;
            init_logging(log_config);
        }

        set_log_level(config.log_level_);

        paths::ensure_directories();

        if (!config.config_file_.empty() && !std::filesystem::exists(config.config_file_)) {
            LOG_INFO("Creating default config file: " + config.config_file_.string());
            ConfigParser::save_to_file(config, config.config_file_);
        }

        Application app(config);
        if (!app.run()) {
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
