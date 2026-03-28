#include "config.h"

#include <fstream>
#include <iostream>
#include <print>

#include "logger.h"
#include "xdg.h"

namespace ip_server {

namespace {

bool parse_bool_string(const std::string& value) {
    return value == "true" || value == "1";
}

void apply_xdg_defaults(ServerConfig& config) {
    auto& xdg            = XDGPaths::instance();
    config.city_db_path  = xdg.city_db_path().string();
    config.asn_db_path   = xdg.asn_db_path().string();
    config.oui_db_path   = xdg.oui_db_path().string();
    config.config_file   = xdg.config_file();
    config.log_file_path = xdg.log_file_path().string();
}

}  // namespace

ServerConfig ConfigParser::default_config() {
    ServerConfig config;
    config.host             = "0.0.0.0";
    config.port             = 8080;
    config.thread_pool_size = 4;
    return config;
}

ServerConfig ConfigParser::parse(int argc, char* argv[]) {
    ServerConfig config = default_config();

    try {
        cxxopts::Options options("ip_server", "IP Geolocation & AS Lookup Service");

        // Positional options
        options.add_options()("help,h", "Show this help message")("config",
                                                                  "Path to configuration file",
                                                                  cxxopts::value<std::string>())(
            "city-db", "Path to City MaxMind database",
            cxxopts::value<std::string>())("asn-db", "Path to ASN MaxMind database",
                                           cxxopts::value<
                                               std::string>())("oui-db", "Path to OUI database",
                                                               cxxopts::value<std::string>())(
            "host", "Server host address",
            cxxopts::value<std::string>()->default_value(
                "0.0.0.0"))("port", "Server port",
                            cxxopts::value<uint16_t>()->default_value(
                                "8080"))("threads", "Thread pool size",
                                         cxxopts::value<int>()->default_value(
                                             "4"))("cache-size", "Cache size",
                                                   cxxopts::value<size_t>()->default_value(
                                                       "10000"))(
            "enable-rate-limiter", "Enable rate limiting",
            cxxopts::value<std::string>()->default_value(
                "true"))("max-requests-per-minute", "Maximum requests per IP per minute",
                         cxxopts::value<int>()->default_value(
                             "100"))("max-batch-size", "Maximum batch size for batch lookup",
                                     cxxopts::value<int>()->default_value(
                                         "100"))("enable-api-auth", "Enable API authentication",
                                                 cxxopts::value<std::string>()->default_value(
                                                     "false"))("api-keys-file",
                                                               "Path to file containing API keys",
                                                               cxxopts::value<std::string>())(
            "default-api-key", "Default API key for testing",
            cxxopts::value<std::string>())("enable-file-logging", "Enable file logging",
                                           cxxopts::value<std::string>()->default_value(
                                               "false"))("log-file", "Path to log file",
                                                         cxxopts::value<std::string>()
                                                             ->default_value("logs/ip_server.log"))(
            "log-enable-stdout", "Enable stdout logging",
            cxxopts::value<std::string>()->default_value(
                "true"))("log-rotation", "Log rotation type: none, size, time, both",
                         cxxopts::value<std::string>()->default_value(
                             "size"))("log-max-size", "Maximum log file size in MB",
                                      cxxopts::value<size_t>()->default_value(
                                          "10"))("log-rotation-interval",
                                                 "Time interval in minutes for time-based rotation",
                                                 cxxopts::value<int>()->default_value("1440"))(
            "log-max-backups", "Maximum number of backup log files",
            cxxopts::value<int>()->default_value(
                "5"))("log-level", "Log level: trace, debug, info, warn, error, critical, off",
                      cxxopts::value<std::string>()->default_value("info"));

        auto result = options.parse(argc, argv);

        // Check for help
        if (result.count("help") != 0u) {
            std::cout << options.help() << '\n';
            std::exit(0);
        }

        // Apply XDG defaults
        apply_xdg_defaults(config);

        LOG_INFO("Using XDG paths:");
        LOG_INFO("  Config: " + config.config_file.string());
        LOG_INFO("  City DB: " + config.city_db_path);
        LOG_INFO("  ASN DB: " + config.asn_db_path);
        LOG_INFO("  OUI DB: " + config.oui_db_path);
        LOG_INFO("  Log File: " + config.log_file_path);

        // Check if config file is specified
        if (result.count("config") != 0u) {
            config.config_file = result["config"].as<std::string>();
        }

        // Load from config file if it exists
        if (!config.config_file.empty() && std::filesystem::exists(config.config_file)) {
            // Check if config file is empty
            std::ifstream check_file(config.config_file);
            bool is_empty = check_file.peek() == std::ifstream::traits_type::eof();
            check_file.close();

            if (is_empty) {
                LOG_INFO("Config file is empty, creating default configuration: "
                         + config.config_file.string());
                ServerConfig default_cfg = default_config();
                // Apply XDG defaults to default config
                apply_xdg_defaults(default_cfg);
                default_cfg.config_file = config.config_file;

                if (save_to_file(default_cfg, config.config_file)) {
                    config = default_cfg;
                    LOG_INFO("Created default configuration file: " + config.config_file.string());
                } else {
                    LOG_WARNING("Failed to create default config file, using built-in defaults");
                }
            } else {
                try {
                    config = load_from_file(config.config_file);
                    LOG_INFO("Loaded configuration from: " + config.config_file.string());
                } catch (const std::exception& e) {
                    LOG_WARNING("Failed to load config file: " + std::string(e.what()));
                }
            }
        }

        // Parse command line arguments (override config file)
        if (result.count("host") != 0u) {
            config.host = result["host"].as<std::string>();
        }
        if (result.count("port") != 0u) {
            config.port = result["port"].as<uint16_t>();
        }
        if (result.count("threads") != 0u) {
            config.thread_pool_size = result["threads"].as<int>();
        }
        if (result.count("cache-size") != 0u) {
            config.cache_size = result["cache-size"].as<size_t>();
        }
        if (result.count("city-db") != 0u) {
            config.city_db_path = result["city-db"].as<std::string>();
        }
        if (result.count("asn-db") != 0u) {
            config.asn_db_path = result["asn-db"].as<std::string>();
        }
        if (result.count("oui-db") != 0u) {
            config.oui_db_path = result["oui-db"].as<std::string>();
        }
        if (result.count("enable-rate-limiter") != 0u) {
            config.enable_rate_limiter =
                parse_bool_string(result["enable-rate-limiter"].as<std::string>());
        }
        if (result.count("max-requests-per-minute") != 0u) {
            config.max_requests_per_minute = result["max-requests-per-minute"].as<int>();
        }
        if (result.count("max-batch-size") != 0u) {
            config.max_batch_size = result["max-batch-size"].as<int>();
        }
        if (result.count("enable-api-auth") != 0u) {
            config.enable_api_auth = parse_bool_string(result["enable-api-auth"].as<std::string>());
        }
        if (result.count("api-keys-file") != 0u) {
            config.api_keys_file = result["api-keys-file"].as<std::string>();
        }
        if (result.count("default-api-key") != 0u) {
            config.default_api_key = result["default-api-key"].as<std::string>();
        }
        if (result.count("enable-file-logging") != 0u) {
            config.enable_file_logging =
                parse_bool_string(result["enable-file-logging"].as<std::string>());
            if (config.enable_file_logging && config.log_file_path == "logs/ip_server.log") {
                config.log_file_path = XDGPaths::instance().log_file_path().string();
            }
        }
        if (result.count("log-file") != 0u) {
            config.log_file_path       = result["log-file"].as<std::string>();
            config.enable_file_logging = true;
        }
        if (result.count("log-enable-stdout") != 0u) {
            config.log_enable_stdout =
                parse_bool_string(result["log-enable-stdout"].as<std::string>());
        }
        if (result.count("log-rotation") != 0u) {
            config.log_rotation_type = result["log-rotation"].as<std::string>();
        }
        if (result.count("log-max-size") != 0u) {
            config.log_max_file_size = result["log-max-size"].as<size_t>() * 1024 * 1024;
        }
        if (result.count("log-rotation-interval") != 0u) {
            config.log_rotation_interval_minutes = result["log-rotation-interval"].as<int>();
        }
        if (result.count("log-max-backups") != 0u) {
            config.log_max_backup_files = result["log-max-backups"].as<int>();
        }
        if (result.count("log-level") != 0u) {
            config.log_level = result["log-level"].as<std::string>();
        }

    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing options: " << e.what() << '\n';
        throw std::runtime_error(std::string("Failed to parse command line options: ") + e.what());
    }

    LOG_INFO("Configuration loaded:");
    LOG_INFO("  Host: " + config.host);
    LOG_INFO("  Port: " + std::to_string(config.port));
    LOG_INFO("  City DB: " + config.city_db_path);
    LOG_INFO("  ASN DB: " + config.asn_db_path);
    LOG_INFO("  OUI DB: " + config.oui_db_path);
    LOG_INFO("  Threads: " + std::to_string(config.thread_pool_size));
    LOG_INFO("  Rate Limiter: " + std::string(config.enable_rate_limiter ? "enabled" : "disabled"));
    if (config.enable_rate_limiter) {
        LOG_INFO("  Max Requests/Min: " + std::to_string(config.max_requests_per_minute));
    }
    LOG_INFO("  Max Batch Size: " + std::to_string(config.max_batch_size));
    LOG_INFO("  API Auth: " + std::string(config.enable_api_auth ? "enabled" : "disabled"));
    if (config.enable_api_auth) {
        LOG_INFO("  API Keys File: " + config.api_keys_file);
        if (!config.default_api_key.empty()) {
            LOG_INFO("  Default API Key: " + config.default_api_key.substr(0, 8) + "...");
        }
    }
    LOG_INFO("  File Logging: " + std::string(config.enable_file_logging ? "enabled" : "disabled"));
    if (config.enable_file_logging) {
        LOG_INFO("  Log File: " + config.log_file_path);
        LOG_INFO("  Log Rotation: " + config.log_rotation_type);
        if (config.log_rotation_type == "size" || config.log_rotation_type == "both") {
            LOG_INFO("  Max File Size: " + std::to_string(config.log_max_file_size / (1024 * 1024))
                     + " MB");
        }
        if (config.log_rotation_type == "time" || config.log_rotation_type == "both") {
            LOG_INFO("  Rotation Interval: " + std::to_string(config.log_rotation_interval_minutes)
                     + " minutes");
        }
        LOG_INFO("  Max Backup Files: " + std::to_string(config.log_max_backup_files));
    }
    LOG_INFO("  Stdout Logging: " + std::string(config.log_enable_stdout ? "enabled" : "disabled"));
    LOG_INFO("  Log Level: " + config.log_level);

    // Validate configuration
    validate(config);

    return config;
}

void ConfigParser::validate(const ServerConfig& config) {
    // Validate port range
    if (config.port < 1) {
        LOG_ERROR("Port must be between 1 and 65535, got: " + std::to_string(config.port));
        throw std::runtime_error("Invalid port number: must be between 1 and 65535");
    }

    // Validate thread pool size
    if (config.thread_pool_size < 1 || config.thread_pool_size > 64) {
        LOG_ERROR("Thread pool size must be between 1 and 64, got: "
                  + std::to_string(config.thread_pool_size));
        throw std::runtime_error("Invalid thread pool size: must be between 1 and 64");
    }

    // Validate cache size
    if (config.cache_size > 1000000) {
        LOG_ERROR("Cache size must be between 0 and 1000000, got: "
                  + std::to_string(config.cache_size));
        throw std::runtime_error("Invalid cache size: must be between 0 and 1000000");
    }

    // Validate rate limiter settings
    if (config.enable_rate_limiter) {
        if (config.max_requests_per_minute < 1 || config.max_requests_per_minute > 10000) {
            LOG_ERROR("Max requests per minute must be between 1 and 10000, got: "
                      + std::to_string(config.max_requests_per_minute));
            throw std::runtime_error(
                "Invalid max requests per minute: must be between 1 and 10000");
        }
    }

    // Validate batch size limit
    if (config.max_batch_size < 1 || config.max_batch_size > 1000) {
        LOG_ERROR("Max batch size must be between 1 and 1000, got: "
                  + std::to_string(config.max_batch_size));
        throw std::runtime_error("Invalid max batch size: must be between 1 and 1000");
    }

    // Validate database paths
    for (const auto& [name, path] :
         {std::make_pair("City", config.city_db_path), std::make_pair("ASN", config.asn_db_path),
          std::make_pair("OUI", config.oui_db_path)}) {
        if (!path.empty() && !std::filesystem::exists(path)) {
            LOG_WARNING(name + std::string(" database file does not exist: ") + path);
        }
    }

    // Validate logging configuration
    if (config.enable_file_logging) {
        if (config.log_rotation_type != "none" && config.log_rotation_type != "size"
            && config.log_rotation_type != "time" && config.log_rotation_type != "both") {
            LOG_ERROR("Invalid log rotation type: " + config.log_rotation_type);
            throw std::runtime_error(
                "Invalid log rotation type: must be none, size, time, or both");
        }

        if (config.log_max_file_size < 1024 * 1024
            || config.log_max_file_size > 1024 * 1024 * 1024) {
            LOG_ERROR("Log max file size must be between 1 MB and 1 GB, got: "
                      + std::to_string(config.log_max_file_size / (1024 * 1024)) + " MB");
            throw std::runtime_error("Invalid log max file size: must be between 1 MB and 1 GB");
        }

        if (config.log_rotation_interval_minutes < 1
            || config.log_rotation_interval_minutes > 10080) {
            LOG_ERROR(
                "Log rotation interval must be between 1 and 10080 minutes (1 "
                "week), got: "
                + std::to_string(config.log_rotation_interval_minutes));
            throw std::runtime_error(
                "Invalid log rotation interval: must be between 1 and 10080 minutes");
        }

        if (config.log_max_backup_files < 0 || config.log_max_backup_files > 100) {
            LOG_ERROR("Log max backup files must be between 0 and 100, got: "
                      + std::to_string(config.log_max_backup_files));
            throw std::runtime_error("Invalid log max backup files: must be between 0 and 100");
        }
    }

    LOG_INFO("Configuration validation passed");
}

ServerConfig ConfigParser::load_from_file(const std::filesystem::path& config_file) {
    ServerConfig config = default_config();

    std::ifstream file(config_file);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_file.string());
    }

    nlohmann::json j;
    file >> j;

    // Parse JSON fields
    if (j.contains("host")) {
        config.host = j["host"].get<std::string>();
    }
    if (j.contains("port")) {
        config.port = j["port"].get<uint16_t>();
    }
    if (j.contains("city_db")) {
        config.city_db_path = j["city_db"].get<std::string>();
    }
    if (j.contains("asn_db")) {
        config.asn_db_path = j["asn_db"].get<std::string>();
    }
    if (j.contains("oui_db")) {
        config.oui_db_path = j["oui_db"].get<std::string>();
    }
    if (j.contains("threads")) {
        config.thread_pool_size = j["threads"].get<int>();
    }
    if (j.contains("cache_size")) {
        config.cache_size = j["cache_size"].get<size_t>();
    }
    if (j.contains("enable_rate_limiter")) {
        config.enable_rate_limiter = j["enable_rate_limiter"].get<bool>();
    }
    if (j.contains("max_requests_per_minute")) {
        config.max_requests_per_minute = j["max_requests_per_minute"].get<int>();
    }
    if (j.contains("max_batch_size")) {
        config.max_batch_size = j["max_batch_size"].get<int>();
    }
    if (j.contains("enable_api_auth")) {
        config.enable_api_auth = j["enable_api_auth"].get<bool>();
    }
    if (j.contains("api_keys_file")) {
        config.api_keys_file = j["api_keys_file"].get<std::string>();
    }
    if (j.contains("default_api_key")) {
        config.default_api_key = j["default_api_key"].get<std::string>();
    }
    if (j.contains("enable_file_logging")) {
        config.enable_file_logging = j["enable_file_logging"].get<bool>();
    }
    if (j.contains("log_file")) {
        config.log_file_path = j["log_file"].get<std::string>();
    }
    if (j.contains("log_enable_stdout")) {
        config.log_enable_stdout = j["log_enable_stdout"].get<bool>();
    }
    if (j.contains("log_rotation")) {
        config.log_rotation_type = j["log_rotation"].get<std::string>();
    }
    if (j.contains("log_max_file_size")) {
        config.log_max_file_size = j["log_max_file_size"].get<size_t>() * 1024 * 1024;
    }
    if (j.contains("log_rotation_interval_minutes")) {
        config.log_rotation_interval_minutes = j["log_rotation_interval_minutes"].get<int>();
    }
    if (j.contains("log_max_backup_files")) {
        config.log_max_backup_files = j["log_max_backup_files"].get<int>();
    }
    if (j.contains("log_level")) {
        config.log_level = j["log_level"].get<std::string>();
    }

    LOG_INFO("Loaded JSON configuration from: " + config_file.string());
    return config;
}

bool ConfigParser::save_to_file(const ServerConfig& config,
                                const std::filesystem::path& config_file) {
    try {
        nlohmann::json j;

        // Build JSON object
        j["host"]                    = config.host;
        j["port"]                    = config.port;
        j["city_db"]                 = config.city_db_path;
        j["asn_db"]                  = config.asn_db_path;
        j["oui_db"]                  = config.oui_db_path;
        j["threads"]                 = config.thread_pool_size;
        j["cache_size"]              = config.cache_size;
        j["enable_rate_limiter"]     = config.enable_rate_limiter;
        j["max_requests_per_minute"] = config.max_requests_per_minute;
        j["max_batch_size"]          = config.max_batch_size;
        j["enable_api_auth"]         = config.enable_api_auth;
        j["api_keys_file"]           = config.api_keys_file;
        if (!config.default_api_key.empty()) {
            j["default_api_key"] = config.default_api_key;
        }
        j["enable_file_logging"] = config.enable_file_logging;
        j["log_file"]            = config.log_file_path;
        j["log_enable_stdout"]   = config.log_enable_stdout;
        j["log_rotation"]        = config.log_rotation_type;
        j["log_max_file_size"]   = config.log_max_file_size / (1024 * 1024);  // Convert to MB
        j["log_rotation_interval_minutes"] = config.log_rotation_interval_minutes;
        j["log_max_backup_files"]          = config.log_max_backup_files;
        j["log_level"]                     = config.log_level;

        // Write to file with pretty formatting
        std::ofstream file(config_file);
        if (!file.is_open()) {
            LOG_ERROR("Cannot open config file for writing: " + config_file.string());
            return false;
        }

        file << j.dump(2) << '\n';

        LOG_INFO("Configuration saved to: " + config_file.string());
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save configuration: " + std::string(e.what()));
        return false;
    }
}

}  // namespace ip_server