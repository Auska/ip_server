#pragma once

#include <cstdint>
#include <cxxopts.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace ip_server {

// Validation limits
namespace config_limits {
constexpr int    MIN_PORT                  = 1;
constexpr int    MAX_PORT                  = 65535;
constexpr int    MIN_THREAD_POOL           = 1;
constexpr int    MAX_THREAD_POOL           = 64;
constexpr size_t MAX_CACHE_SIZE            = 1000000;
constexpr int    MIN_RATE_LIMIT            = 1;
constexpr int    MAX_RATE_LIMIT            = 10000;
constexpr int    MIN_BATCH_SIZE            = 1;
constexpr int    MAX_BATCH_SIZE            = 1000;
constexpr size_t MIN_LOG_FILE_SIZE = 1 * 1024 * 1024;        // 1 MB
constexpr size_t MAX_LOG_FILE_SIZE = 1 * 1024 * 1024 * 1024;  // 1 GB
constexpr int    MIN_BACKUP_FILES  = 0;
constexpr int    MAX_BACKUP_FILES  = 100;
}  // namespace config_limits

struct ServerConfig {
    std::string host_ = "0.0.0.0";
    uint16_t port_    = 8080;
    std::string city_db_path_;
    std::string asn_db_path_;
    std::string oui_db_path_;
    int thread_pool_size_ = 4;
    size_t cache_size_    = 10000;
    std::filesystem::path config_file_;
    bool enable_rate_limiter_    = true;
    int max_requests_per_minute_ = 100;
    int max_batch_size_          = 100;
    bool enable_api_auth_        = false;
    std::string api_keys_file_;
    std::string default_api_key_;

    // Logging configuration
    bool enable_file_logging_  = false;
    std::string log_file_path_ = "logs/ip_server.log";  // Overridden by default paths
    std::string log_rotation_type_ = "size";            // "none" or "size"
    size_t log_max_file_size_  = 10 * 1024 * 1024;      // 10 MB
    int log_max_backup_files_  = 5;
    bool log_enable_stdout_    = true;
    std::string log_level_ = "info";  // "trace", "debug", "info", "warn", "error", "critical", "off"
};

class ConfigParser {
   public:
    static ServerConfig parse(int argc, char* argv[]);
    static ServerConfig load_from_file(const std::filesystem::path& config_file);
    static bool save_to_file(const ServerConfig& config, const std::filesystem::path& config_file);
    static void validate(const ServerConfig& config);

   private:
    static cxxopts::Options create_option_parser();
    static void from_json(ServerConfig& config, const nlohmann::json& j);
    static void apply_cli_overrides(const cxxopts::ParseResult& result, ServerConfig& config);
    static void log_config(const ServerConfig& config);
    static ServerConfig handle_config_file(const std::filesystem::path& config_file);
};

}  // namespace ip_server