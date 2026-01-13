#include "config.h"
#include "xdg.h"
#include "logger.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <print>

namespace ip_server {

ServerConfig ConfigParser::default_config() {
    ServerConfig config;
    config.host = "0.0.0.0";
    config.port = 8080;
    config.thread_pool_size = 4;
    config.use_xdg = true;
    return config;
}

void ConfigParser::apply_xdg_defaults(ServerConfig& config) {
    if (config.use_xdg) {
        auto& xdg = XDGPaths::instance();
        config.city_db_path = xdg.city_db_path().string();
        config.asn_db_path = xdg.asn_db_path().string();
        config.oui_db_path = xdg.oui_db_path().string();
        config.config_file = xdg.config_file();
        config.log_file_path = xdg.log_file_path().string();

        LOG_INFO("Using XDG paths:");
        LOG_INFO("  Config: " + config.config_file.string());
        LOG_INFO("  City DB: " + config.city_db_path);
        LOG_INFO("  ASN DB: " + config.asn_db_path);
        LOG_INFO("  OUI DB: " + config.oui_db_path);
        LOG_INFO("  Log File: " + config.log_file_path);
    }
}

ServerConfig ConfigParser::parse(int argc, char* argv[]) {
    ServerConfig config = default_config();

    try {
        cxxopts::Options options("ip_server", "IP Geolocation & AS Lookup Service");

        // Positional options
        options.add_options()
            ("help,h", "Show this help message")
            ("config", "Path to configuration file", cxxopts::value<std::string>())
            ("no-xdg", "Disable XDG directory standard", cxxopts::value<bool>()->default_value("true")->implicit_value("false"))
            ("city-db", "Path to City MaxMind database", cxxopts::value<std::string>())
            ("asn-db", "Path to ASN MaxMind database", cxxopts::value<std::string>())
            ("oui-db", "Path to OUI database", cxxopts::value<std::string>())
            ("host", "Server host address", cxxopts::value<std::string>()->default_value("0.0.0.0"))
            ("port", "Server port", cxxopts::value<uint16_t>()->default_value("8080"))
            ("threads", "Thread pool size", cxxopts::value<int>()->default_value("4"))
            ("cache-size", "Cache size", cxxopts::value<size_t>()->default_value("10000"))
            ("enable-rate-limiter", "Enable rate limiting", cxxopts::value<std::string>()->default_value("true"))
            ("max-requests-per-minute", "Maximum requests per IP per minute", cxxopts::value<int>()->default_value("100"))
            ("max-batch-size", "Maximum batch size for batch lookup", cxxopts::value<int>()->default_value("100"))
            ("enable-api-auth", "Enable API authentication", cxxopts::value<std::string>()->default_value("false"))
            ("api-keys-file", "Path to file containing API keys", cxxopts::value<std::string>())
            ("default-api-key", "Default API key for testing", cxxopts::value<std::string>())
            ("enable-file-logging", "Enable file logging", cxxopts::value<std::string>()->default_value("false"))
            ("log-file", "Path to log file", cxxopts::value<std::string>()->default_value("logs/ip_server.log"))
            ("log-enable-stdout", "Enable stdout logging", cxxopts::value<std::string>()->default_value("true"))
            ("log-rotation", "Log rotation type: none, size, time, both", cxxopts::value<std::string>()->default_value("size"))
            ("log-max-size", "Maximum log file size in MB", cxxopts::value<size_t>()->default_value("10"))
            ("log-rotation-interval", "Time interval in minutes for time-based rotation", cxxopts::value<int>()->default_value("1440"))
            ("log-max-backups", "Maximum number of backup log files", cxxopts::value<int>()->default_value("5"))
            ("log-level", "Log level: trace, debug, info, warn, error, critical, off", cxxopts::value<std::string>()->default_value("info"));

        auto result = options.parse(argc, argv);

        // Check for help
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            std::exit(0);
        }

        // Check for --no-xdg flag first
        if (result.count("no-xdg")) {
            config.use_xdg = result["no-xdg"].as<bool>();
        }

        // Apply XDG defaults only if enabled
        if (config.use_xdg) {
            apply_xdg_defaults(config);
        }

        // Check if config file is specified
        if (result.count("config")) {
            config.config_file = result["config"].as<std::string>();
            config.use_xdg = false;
        }

        // Load from config file if it exists
        if (!config.config_file.empty() && std::filesystem::exists(config.config_file)) {
            try {
                config = load_from_file(config.config_file);
                LOG_INFO("Loaded configuration from: " + config.config_file.string());
            } catch (const std::exception& e) {
                LOG_WARNING("Failed to load config file: " + std::string(e.what()));
            }
        }

        // Parse command line arguments (override config file)
        if (result.count("host")) {
            config.host = result["host"].as<std::string>();
        }
        if (result.count("port")) {
            config.port = result["port"].as<uint16_t>();
        }
        if (result.count("threads")) {
            config.thread_pool_size = result["threads"].as<int>();
        }
        if (result.count("cache-size")) {
            config.cache_size = result["cache-size"].as<size_t>();
        }
        if (result.count("city-db")) {
            config.city_db_path = result["city-db"].as<std::string>();
            config.use_xdg = false;
        }
        if (result.count("asn-db")) {
            config.asn_db_path = result["asn-db"].as<std::string>();
            config.use_xdg = false;
        }
        if (result.count("oui-db")) {
            config.oui_db_path = result["oui-db"].as<std::string>();
            config.use_xdg = false;
        }
        if (result.count("enable-rate-limiter")) {
            std::string value = result["enable-rate-limiter"].as<std::string>();
            config.enable_rate_limiter = (value == "true" || value == "1");
        }
        if (result.count("max-requests-per-minute")) {
            config.max_requests_per_minute = result["max-requests-per-minute"].as<int>();
        }
        if (result.count("max-batch-size")) {
            config.max_batch_size = result["max-batch-size"].as<int>();
        }
        if (result.count("enable-api-auth")) {
            std::string value = result["enable-api-auth"].as<std::string>();
            config.enable_api_auth = (value == "true" || value == "1");
        }
        if (result.count("api-keys-file")) {
            config.api_keys_file = result["api-keys-file"].as<std::string>();
        }
        if (result.count("default-api-key")) {
            config.default_api_key = result["default-api-key"].as<std::string>();
        }
        if (result.count("enable-file-logging")) {
            std::string value = result["enable-file-logging"].as<std::string>();
            config.enable_file_logging = (value == "true" || value == "1");
            // If enabling file logging and using XDG, use XDG log path
            if (config.enable_file_logging && config.use_xdg && config.log_file_path == "logs/ip_server.log") {
                config.log_file_path = XDGPaths::instance().log_file_path().string();
            }
        }
        if (result.count("log-file")) {
            config.log_file_path = result["log-file"].as<std::string>();
            config.enable_file_logging = true;
        }
        if (result.count("log-enable-stdout")) {
            std::string value = result["log-enable-stdout"].as<std::string>();
            config.log_enable_stdout = (value == "true" || value == "1");
        }
        if (result.count("log-rotation")) {
            config.log_rotation_type = result["log-rotation"].as<std::string>();
        }
        if (result.count("log-max-size")) {
            config.log_max_file_size = result["log-max-size"].as<size_t>() * 1024 * 1024;
        }
        if (result.count("log-rotation-interval")) {
            config.log_rotation_interval_minutes = result["log-rotation-interval"].as<int>();
        }
        if (result.count("log-max-backups")) {
            config.log_max_backup_files = result["log-max-backups"].as<int>();
        }
        if (result.count("log-level")) {
            config.log_level = result["log-level"].as<std::string>();
        }

    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        throw std::runtime_error(std::string("Failed to parse command line options: ") + e.what());
    }

    LOG_INFO("Configuration loaded:");
    LOG_INFO("  Host: " + config.host);
    LOG_INFO("  Port: " + std::to_string(config.port));
    LOG_INFO("  City DB: " + config.city_db_path);
    LOG_INFO("  ASN DB: " + config.asn_db_path);
    LOG_INFO("  OUI DB: " + config.oui_db_path);
    LOG_INFO("  Threads: " + std::to_string(config.thread_pool_size));
    LOG_INFO("  Use XDG: " + std::string(config.use_xdg ? "yes" : "no"));
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
            LOG_INFO("  Max File Size: " + std::to_string(config.log_max_file_size / (1024 * 1024)) + " MB");
        }
        if (config.log_rotation_type == "time" || config.log_rotation_type == "both") {
            LOG_INFO("  Rotation Interval: " + std::to_string(config.log_rotation_interval_minutes) + " minutes");
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
        LOG_ERROR("Thread pool size must be between 1 and 64, got: " + std::to_string(config.thread_pool_size));
        throw std::runtime_error("Invalid thread pool size: must be between 1 and 64");
    }

    // Validate cache size
    if (config.cache_size > 1000000) {
        LOG_ERROR("Cache size must be between 0 and 1000000, got: " + std::to_string(config.cache_size));
        throw std::runtime_error("Invalid cache size: must be between 0 and 1000000");
    }

    // Validate rate limiter settings
    if (config.enable_rate_limiter) {
        if (config.max_requests_per_minute < 1 || config.max_requests_per_minute > 10000) {
            LOG_ERROR("Max requests per minute must be between 1 and 10000, got: " + 
                     std::to_string(config.max_requests_per_minute));
            throw std::runtime_error("Invalid max requests per minute: must be between 1 and 10000");
        }
    }

    // Validate batch size limit
    if (config.max_batch_size < 1 || config.max_batch_size > 1000) {
        LOG_ERROR("Max batch size must be between 1 and 1000, got: " + std::to_string(config.max_batch_size));
        throw std::runtime_error("Invalid max batch size: must be between 1 and 1000");
    }

    // Validate database paths
    if (!config.city_db_path.empty() && !std::filesystem::exists(config.city_db_path)) {
        LOG_WARNING("City database file does not exist: " + config.city_db_path);
    }

    if (!config.asn_db_path.empty() && !std::filesystem::exists(config.asn_db_path)) {
        LOG_WARNING("ASN database file does not exist: " + config.asn_db_path);
    }

    // Validate logging configuration
    if (config.enable_file_logging) {
        if (config.log_rotation_type != "none" && 
            config.log_rotation_type != "size" && 
            config.log_rotation_type != "time" && 
            config.log_rotation_type != "both") {
            LOG_ERROR("Invalid log rotation type: " + config.log_rotation_type);
            throw std::runtime_error("Invalid log rotation type: must be none, size, time, or both");
        }
        
        if (config.log_max_file_size < 1024 * 1024 || config.log_max_file_size > 1024 * 1024 * 1024) {
            LOG_ERROR("Log max file size must be between 1 MB and 1 GB, got: " + 
                     std::to_string(config.log_max_file_size / (1024 * 1024)) + " MB");
            throw std::runtime_error("Invalid log max file size: must be between 1 MB and 1 GB");
        }
        
        if (config.log_rotation_interval_minutes < 1 || config.log_rotation_interval_minutes > 10080) {
            LOG_ERROR("Log rotation interval must be between 1 and 10080 minutes (1 week), got: " + 
                     std::to_string(config.log_rotation_interval_minutes));
            throw std::runtime_error("Invalid log rotation interval: must be between 1 and 10080 minutes");
        }
        
        if (config.log_max_backup_files < 0 || config.log_max_backup_files > 100) {
            LOG_ERROR("Log max backup files must be between 0 and 100, got: " + 
                     std::to_string(config.log_max_backup_files));
            throw std::runtime_error("Invalid log max backup files: must be between 0 and 100");
        }
    }

    LOG_INFO("Configuration validation passed");
}

ServerConfig ConfigParser::load_from_file(const std::filesystem::path& config_file) {
    ServerConfig config = default_config();

    // Try to parse as JSON first
    try {
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

    } catch (const nlohmann::json::exception& e) {
        // If JSON parsing fails, try the old format
        LOG_WARNING("JSON parsing failed, trying legacy format: " + std::string(e.what()));
    }

    // Fallback to legacy key=value format
    std::ifstream file(config_file);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_file.string());
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Parse key = value
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // Apply values
        if (key == "host") {
            config.host = value;
        } else if (key == "port") {
            config.port = static_cast<uint16_t>(std::stoi(value));
        } else if (key == "city_db") {
            config.city_db_path = value;
        } else if (key == "asn_db") {
            config.asn_db_path = value;
        } else if (key == "oui_db") {
            config.oui_db_path = value;
        } else if (key == "threads") {
            config.thread_pool_size = std::stoi(value);
        } else if (key == "enable_rate_limiter") {
            config.enable_rate_limiter = (value == "true" || value == "1");
        } else if (key == "max_requests_per_minute") {
            config.max_requests_per_minute = std::stoi(value);
        } else if (key == "max_batch_size") {
            config.max_batch_size = std::stoi(value);
        } else if (key == "enable_api_auth") {
            config.enable_api_auth = (value == "true" || value == "1");
        } else if (key == "api_keys_file") {
            config.api_keys_file = value;
        } else if (key == "default_api_key") {
            config.default_api_key = value;
        } else if (key == "enable_file_logging") {
            config.enable_file_logging = (value == "true" || value == "1");
        } else if (key == "log_enable_stdout") {
            config.log_enable_stdout = (value == "true" || value == "1");
        } else if (key == "log_file") {
            config.log_file_path = value;
            config.enable_file_logging = true;
        } else if (key == "log_rotation") {
            config.log_rotation_type = value;
        } else if (key == "log_max_file_size") {
            config.log_max_file_size = std::stoull(value) * 1024 * 1024;
        } else if (key == "log_rotation_interval_minutes") {
            config.log_rotation_interval_minutes = std::stoi(value);
        } else if (key == "log_max_backup_files") {
            config.log_max_backup_files = std::stoi(value);
        } else if (key == "log_level") {
            config.log_level = value;
        }
    }

    LOG_INFO("Loaded legacy configuration from: " + config_file.string());
    return config;
}

bool ConfigParser::save_to_file(const ServerConfig& config, const std::filesystem::path& config_file) {
    try {
        nlohmann::json j;

        // Build JSON object
        j["host"] = config.host;
        j["port"] = config.port;
        j["city_db"] = config.city_db_path;
        j["asn_db"] = config.asn_db_path;
        j["oui_db"] = config.oui_db_path;
        j["threads"] = config.thread_pool_size;
        j["cache_size"] = config.cache_size;
        j["enable_rate_limiter"] = config.enable_rate_limiter;
        j["max_requests_per_minute"] = config.max_requests_per_minute;
        j["max_batch_size"] = config.max_batch_size;
        j["enable_api_auth"] = config.enable_api_auth;
        j["api_keys_file"] = config.api_keys_file;
        if (!config.default_api_key.empty()) {
            j["default_api_key"] = config.default_api_key;
        }
        j["enable_file_logging"] = config.enable_file_logging;
        j["log_file"] = config.log_file_path;
        j["log_enable_stdout"] = config.log_enable_stdout;
        j["log_rotation"] = config.log_rotation_type;
        j["log_max_file_size"] = config.log_max_file_size / (1024 * 1024);  // Convert to MB
        j["log_rotation_interval_minutes"] = config.log_rotation_interval_minutes;
        j["log_max_backup_files"] = config.log_max_backup_files;
        j["log_level"] = config.log_level;

        // Write to file with pretty formatting
        std::ofstream file(config_file);
        if (!file.is_open()) {
            LOG_ERROR("Cannot open config file for writing: " + config_file.string());
            return false;
        }

        file << j.dump(2) << std::endl;
        file.close();

        LOG_INFO("Configuration saved to: " + config_file.string());
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save configuration: " + std::string(e.what()));
        return false;
    }
}

void ConfigParser::print_help(const char* program_name) {
    std::print("Usage: {} [options]\n", program_name);
    std::print("\n");
    std::print("IP Geolocation & AS Lookup Service\n");
    std::print("\n");
    std::print("Options:\n");
    std::print("  --config <path>      Path to configuration file\n");
    std::print("  --city-db <path>     Path to City MaxMind database\n");
    std::print("                      (default: ~/.local/share/ip-server/databases/GeoLite2-City.mmdb)\n");
    std::print("  --asn-db <path>      Path to ASN MaxMind database\n");
    std::print("                      (default: ~/.local/share/ip-server/databases/GeoLite2-ASN.mmdb)\n");
    std::print("  --host <address>     Server host address\n");
    std::print("                      (default: 0.0.0.0)\n");
    std::print("  --port <port>        Server port\n");
    std::print("                      (default: 8080)\n");
    std::print("  --threads <count>    Thread pool size\n");
    std::print("                      (default: 4)\n");
    std::print("  --enable-rate-limiter <true|false>\n");
    std::print("                      Enable rate limiting\n");
    std::print("                      (default: true)\n");
    std::print("  --max-requests-per-minute <count>\n");
    std::print("                      Maximum requests per IP per minute\n");
    std::print("                      (default: 100)\n");
    std::print("  --max-batch-size <count>\n");
    std::print("                      Maximum batch size for batch lookup\n");
    std::print("                      (default: 100)\n");
    std::print("  --enable-api-auth <true|false>\n");
    std::print("                      Enable API authentication\n");
    std::print("                      (default: false)\n");
    std::print("  --api-keys-file <path>\n");
    std::print("                      Path to file containing API keys (one per line)\n");
    std::print("  --default-api-key <key>\n");
    std::print("                      Default API key for testing\n");
    std::print("  --enable-file-logging <true|false>\n");
    std::print("                      Enable file logging\n");
    std::print("                      (default: false)\n");
    std::print("  --log-enable-stdout <true|false>\n");
    std::print("                      Enable stdout logging\n");
    std::print("                      (default: true)\n");
    std::print("  --log-file <path>    Path to log file\n");
    std::print("                      (default: ~/.local/state/ip-server/logs/ip_server.log)\n");
    std::print("  --log-rotation <type>\n");
    std::print("                      Log rotation type: none, size, time, both\n");
    std::print("                      (default: size)\n");
    std::print("  --log-max-size <MB>  Maximum log file size in MB before rotation\n");
    std::print("                      (default: 10)\n");
    std::print("  --log-rotation-interval <minutes>\n");
    std::print("                      Time interval in minutes for time-based rotation\n");
    std::print("                      (default: 1440, 24 hours)\n");
    std::print("  --log-max-backups <count>\n");
    std::print("                      Maximum number of backup log files to keep\n");
    std::print("                      (default: 5)\n");
    std::print("  --no-xdg             Disable XDG directory standard\n");
    std::print("  --help, -h           Show this help message\n");
    std::print("\n");
    std::print("XDG Directories:\n");
    std::print("  Config:  $XDG_CONFIG_HOME/ip-server/ (default: ~/.config/ip-server/)\n");
    std::print("  Data:    $XDG_DATA_HOME/ip-server/ (default: ~/.local/share/ip-server/)\n");
    std::print("  Cache:   $XDG_CACHE_HOME/ip-server/ (default: ~/.cache/ip-server/)\n");
    std::print("  Logs:    $XDG_STATE_HOME/ip-server/logs/ (default: ~/.local/state/ip-server/logs/)\n");
    std::print("\n");
    std::print("Environment Variables:\n");
    std::print("  XDG_CONFIG_HOME  Configuration directory\n");
    std::print("  XDG_DATA_HOME    Data directory\n");
    std::print("  XDG_CACHE_HOME   Cache directory\n");
    std::print("  XDG_STATE_HOME   State directory (logs)\n");
    std::print("\n");
    std::print("Examples:\n");
    std::print("  {}\n", program_name);
    std::print("  {} --port 9000\n", program_name);
    std::print("  {} --config /path/to/config.toml\n", program_name);
    std::print("  {} --city-db /path/to/GeoLite2-City.mmdb --asn-db /path/to/GeoLite2-ASN.mmdb\n", program_name);
    std::print("  {} --oui-db /path/to/master_oui.db\n", program_name);
    std::print("  {} --host 127.0.0.1 --port 8080\n", program_name);
    std::print("\n");
    std::print("API Endpoints:\n");
    std::print("  GET  /                       - Service information\n");
    std::print("  GET  /health                 - Health check\n");
    std::print("  GET  /lookup?ip=<address>     - Single IP lookup\n");
    std::print("  POST /lookup                 - Batch lookup\n");
    std::print("                                Body: {{\"ips\": [\"1.1.1.1\", \"8.8.8.8\"]}}\n");
    std::print("  GET  /mac/lookup?mac=<address> - Single MAC lookup\n");
    std::print("  POST /mac/lookup             - Batch MAC lookup\n");
    std::print("                                Body: {{\"macs\": [\"00:1A:2B:3C:4D:5E\", \"F4:EA:B5:12:34:56\"]}}\n");
    std::print("\n");
    std::print("For more information, visit: https://dev.maxmind.com/geoip/geolite2-free-geolocation-data\n");
}

} // namespace ip_server