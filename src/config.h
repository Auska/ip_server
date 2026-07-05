#pragma once

#include <cstdint>
#include <cxxopts.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace ip_server {

struct ServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port    = 8080;
    std::string city_db_path;
    std::string asn_db_path;
    std::string oui_db_path;
    int thread_pool_size = 4;
    size_t cache_size    = 10000;
    std::filesystem::path config_file;
    bool enable_rate_limiter    = true;
    int max_requests_per_minute = 100;
    int max_batch_size          = 100;
    bool enable_api_auth        = false;
    std::string api_keys_file;
    std::string default_api_key;

    // Logging configuration
    bool enable_file_logging          = false;
    std::string log_file_path         = "logs/ip_server.log";  // Overridden by XDG paths
    std::string log_rotation_type     = "size";                // "none", "size", "time", "both"
    size_t log_max_file_size          = 10 * 1024 * 1024;      // 10 MB
    int log_rotation_interval_minutes = 1440;                  // 24 hours
    int log_max_backup_files          = 5;
    bool log_enable_stdout            = true;
    std::string log_level = "info";  // "trace", "debug", "info", "warn", "error", "critical", "off"
};

class ConfigParser {
   public:
    static ServerConfig parse(int argc, char* argv[]);
    static ServerConfig default_config();
    static ServerConfig load_from_file(const std::filesystem::path& config_file);
    static bool save_to_file(const ServerConfig& config, const std::filesystem::path& config_file);
    static void validate(const ServerConfig& config);

   private:
    static cxxopts::Options create_option_parser();
    static void apply_cli_overrides(const cxxopts::ParseResult& result, ServerConfig& config);
    static void log_config(const ServerConfig& config);
};

}  // namespace ip_server