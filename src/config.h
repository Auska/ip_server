#pragma once

#include <string>
#include <cstdint>
#include <filesystem>

namespace ip_server {

struct ServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    std::string city_db_path;
    std::string asn_db_path;
    int thread_pool_size = 4;
    size_t cache_size = 10000;
    std::filesystem::path config_file;
    bool use_xdg = true;
    bool enable_rate_limiter = true;
    int max_requests_per_minute = 100;
    int max_batch_size = 100;
    bool enable_api_auth = false;
    std::string api_keys_file;
    std::string default_api_key;
};

class ConfigParser {
public:
    static ServerConfig parse(int argc, char* argv[]);
    static void print_help(const char* program_name);
    static ServerConfig default_config();
    static ServerConfig load_from_file(const std::filesystem::path& config_file);
    static bool save_to_file(const ServerConfig& config, const std::filesystem::path& config_file);
    static void validate(const ServerConfig& config);

private:
    static void apply_xdg_defaults(ServerConfig& config);
};

} // namespace ip_server