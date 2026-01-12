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

    // First, check for --no-xdg flag
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--no-xdg") {
            config.use_xdg = false;
            break;
        }
    }

    // Apply XDG defaults only if enabled
    if (config.use_xdg) {
        apply_xdg_defaults(config);
    }

    // Check if config file is specified
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config.config_file = argv[++i];
            config.use_xdg = false;
            break;
        }
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
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--config" && i + 1 < argc) {
            i++; // Already handled
        } else if (arg == "--city-db" && i + 1 < argc) {
            config.city_db_path = argv[++i];
            config.use_xdg = false;
        } else if (arg == "--asn-db" && i + 1 < argc) {
            config.asn_db_path = argv[++i];
            config.use_xdg = false;
        } else if (arg == "--oui-db" && i + 1 < argc) {
            config.oui_db_path = argv[++i];
            config.use_xdg = false;
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(parse_int_arg(argv[++i], "port number"));
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            config.thread_pool_size = parse_int_arg(argv[++i], "thread count");
        } else if (arg == "--no-xdg") {
            config.use_xdg = false;
        } else if (arg == "--enable-rate-limiter" && i + 1 < argc) {
            std::string value = argv[++i];
            config.enable_rate_limiter = (value == "true" || value == "1");
        } else if (arg == "--max-requests-per-minute" && i + 1 < argc) {
            config.max_requests_per_minute = parse_int_arg(argv[++i], "max requests per minute");
        } else if (arg == "--max-batch-size" && i + 1 < argc) {
            try {
                config.max_batch_size = std::stoi(argv[++i]);
            } catch (const std::exception& e) {
                LOG_ERROR("Invalid max batch size: " + std::string(argv[i]));
                throw std::runtime_error("Invalid max batch size");
            }
        } else if (arg == "--enable-api-auth" && i + 1 < argc) {
            std::string value = argv[++i];
            config.enable_api_auth = (value == "true" || value == "1");
        } else if (arg == "--api-keys-file" && i + 1 < argc) {
            config.api_keys_file = argv[++i];
        } else if (arg == "--default-api-key" && i + 1 < argc) {
            config.default_api_key = argv[++i];
        } else if (arg == "--enable-file-logging" && i + 1 < argc) {
            std::string value = argv[++i];
            config.enable_file_logging = (value == "true" || value == "1");
            // If enabling file logging and using XDG, use XDG log path
            if (config.enable_file_logging && config.use_xdg && config.log_file_path == "logs/ip_server.log") {
                config.log_file_path = XDGPaths::instance().log_file_path().string();
            }
        } else if (arg == "--log-enable-stdout" && i + 1 < argc) {
            std::string value = argv[++i];
            config.log_enable_stdout = (value == "true" || value == "1");
        } else if (arg == "--log-file" && i + 1 < argc) {
            config.log_file_path = argv[++i];
            config.enable_file_logging = true;
        } else if (arg == "--log-rotation" && i + 1 < argc) {
            config.log_rotation_type = argv[++i];
        } else if (arg == "--log-max-size" && i + 1 < argc) {
            config.log_max_file_size = parse_size_arg(argv[++i], "log max file size") * 1024 * 1024;
        } else if (arg == "--log-rotation-interval" && i + 1 < argc) {
            config.log_rotation_interval_minutes = parse_int_arg(argv[++i], "log rotation interval");
        } else if (arg == "--log-max-backups" && i + 1 < argc) {
            config.log_max_backup_files = parse_int_arg(argv[++i], "log max backup files");
        } else if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            std::exit(0);
        } else {
            std::ostringstream oss;
            oss << "Unknown option: " << arg;
            LOG_WARNING(oss.str());
        }
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

    // Simple line-based config parser
    // Format: key = value
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
        }
    }

    return config;
}

bool ConfigParser::save_to_file(const ServerConfig& config, const std::filesystem::path& config_file) {
    std::ofstream file(config_file);
    if (!file.is_open()) {
        LOG_ERROR("Cannot open config file for writing: " + config_file.string());
        return false;
    }

    file << "# IP Geolocation & AS Lookup Service Configuration\n";
    file << "# Generated automatically\n\n";
    file << "host = " << config.host << "\n";
    file << "port = " << config.port << "\n";
    file << "city_db = " << config.city_db_path << "\n";
    file << "asn_db = " << config.asn_db_path << "\n";
    file << "oui_db = " << config.oui_db_path << "\n";
    file << "threads = " << config.thread_pool_size << "\n";
    file << "enable_rate_limiter = " << (config.enable_rate_limiter ? "true" : "false") << "\n";
    file << "max_requests_per_minute = " << config.max_requests_per_minute << "\n";
    file << "max_batch_size = " << config.max_batch_size << "\n";
    file << "enable_api_auth = " << (config.enable_api_auth ? "true" : "false") << "\n";
    file << "api_keys_file = " << config.api_keys_file << "\n";
    if (!config.default_api_key.empty()) {
        file << "default_api_key = " << config.default_api_key << "\n";
    }
    file << "enable_file_logging = " << (config.enable_file_logging ? "true" : "false") << "\n";
    file << "log_enable_stdout = " << (config.log_enable_stdout ? "true" : "false") << "\n";
    if (config.enable_file_logging) {
        file << "log_file = " << config.log_file_path << "\n";
        file << "log_rotation = " << config.log_rotation_type << "\n";
        file << "log_max_file_size = " << (config.log_max_file_size / (1024 * 1024)) << "\n";
        file << "log_rotation_interval_minutes = " << config.log_rotation_interval_minutes << "\n";
        file << "log_max_backup_files = " << config.log_max_backup_files << "\n";
    }

    file.close();
    LOG_INFO("Configuration saved to: " + config_file.string());
    return true;
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

int ConfigParser::parse_int_arg(const char* arg, const char* name) {
    try {
        return std::stoi(arg);
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Invalid ") + name + ": " + arg);
        throw std::runtime_error(std::string("Invalid ") + name);
    }
}

size_t ConfigParser::parse_size_arg(const char* arg, const char* name) {
    try {
        return std::stoull(arg);
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Invalid ") + name + ": " + arg);
        throw std::runtime_error(std::string("Invalid ") + name);
    }
}

} // namespace ip_server