#include <array>
#include <atomic>
#include <csignal>
#include <cstring>
#include <string_view>

#include "config.h"
#include "http_server.h"
#include "logger.h"
#include "metrics.h"
#include "service/ip_geo_service.h"
#include "service/mac_lookup_service.h"
#include "xdg.h"

namespace ip_server {

namespace {

std::atomic<bool> g_shutdown_requested{false};
std::atomic<const Logger*> g_signal_logger{nullptr};

extern "C" void handle_signal(int sig) {
    if (!g_shutdown_requested.exchange(true)) {
        const char* name = (sig == SIGINT) ? "SIGINT" : "SIGTERM";
        if (auto* logger = g_signal_logger.load(std::memory_order_acquire)) {
            logger->signal_safe_log(name);
        }
    }
}

}  // namespace

class Application {
   public:
    Application(const ServerConfig& config)
        : config_(config),
          geo_service_(config.city_db_path, config.asn_db_path, config.cache_size),
          mac_service_(config.oui_db_path, config.cache_size),
          http_server_(config.host, config.port, config.thread_pool_size,
                       config.enable_rate_limiter, config.max_requests_per_minute,
                       config.max_batch_size, config.enable_api_auth, config.api_keys_file,
                       config.default_api_key) {
        http_server_.set_lookup_handler(
            [this](const std::string& ip) { return geo_service_.lookup(ip); });

        http_server_.set_mac_lookup_handler(
            [this](const std::string& mac) { return mac_service_.lookup(mac); });
    }

    bool run() {
        LOG_INFO("Application starting...");

        g_shutdown_requested.store(false);
        g_signal_logger.store(&Logger::instance(), std::memory_order_release);

        struct sigaction sa{};
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_handler = &handle_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);

        if (auto* metrics = http_server_.get_metrics()) {
            metrics->set_city_db_status(geo_service_.is_city_db_open());
            metrics->set_asn_db_status(geo_service_.is_asn_db_open());
            metrics->set_oui_db_status(mac_service_.is_oui_db_open());
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
    ServerConfig config_;
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
            log_config.enable_file_logging = config.enable_file_logging;
            log_config.log_file_path       = config.log_file_path;
            log_config.enable_stdout       = config.log_enable_stdout;

            if (config.log_rotation_type == "size")
                log_config.rotation_type = RotationType::SIZE;
            else if (config.log_rotation_type == "time")
                log_config.rotation_type = RotationType::TIME;
            else if (config.log_rotation_type == "both")
                log_config.rotation_type = RotationType::BOTH;
            else
                log_config.rotation_type = RotationType::NONE;

            log_config.max_file_size = config.log_max_file_size;
            log_config.rotation_interval =
                std::chrono::minutes(config.log_rotation_interval_minutes);
            log_config.max_backup_files = config.log_max_backup_files;

            Logger::instance().set_config(log_config);
        }

        {
            static constexpr auto levels = std::to_array<std::pair<std::string_view, LogLevel>>({
                {"debug", LogLevel::DEBUG},
                {"info", LogLevel::INFO},
                {"warning", LogLevel::WARNING},
                {"error", LogLevel::ERROR},
            });
            for (const auto& [name, level] : levels) {
                if (name == config.log_level) {
                    Logger::instance().set_level(level);
                    break;
                }
            }
        }

        ip_server::XDGPaths::ensure_directories();

        if (!config.config_file.empty() && !std::filesystem::exists(config.config_file)) {
            LOG_INFO("Creating default config file: " + config.config_file.string());
            ConfigParser::save_to_file(config, config.config_file);
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
