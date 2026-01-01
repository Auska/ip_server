#include "config.h"
#include "xdg.h"
#include "logger.h"
#include <iostream>
#include <sstream>
#include <fstream>

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
        config.config_file = xdg.config_file();

        LOG_INFO("Using XDG paths:");
        LOG_INFO("  Config: " + config.config_file.string());
        LOG_INFO("  City DB: " + config.city_db_path);
        LOG_INFO("  ASN DB: " + config.asn_db_path);
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
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
            } catch (const std::exception& e) {
                LOG_ERROR("Invalid port number: " + std::string(argv[i]));
                throw std::runtime_error("Invalid port number");
            }
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            try {
                config.thread_pool_size = std::stoi(argv[++i]);
            } catch (const std::exception& e) {
                LOG_ERROR("Invalid thread count: " + std::string(argv[i]));
                throw std::runtime_error("Invalid thread count");
            }
        } else if (arg == "--no-xdg") {
            config.use_xdg = false;
        } else if (arg == "--enable-rate-limiter" && i + 1 < argc) {
            std::string value = argv[++i];
            config.enable_rate_limiter = (value == "true" || value == "1");
        } else if (arg == "--max-requests-per-minute" && i + 1 < argc) {
            try {
                config.max_requests_per_minute = std::stoi(argv[++i]);
            } catch (const std::exception& e) {
                LOG_ERROR("Invalid max requests per minute: " + std::string(argv[i]));
                throw std::runtime_error("Invalid max requests per minute");
            }
        } else if (arg == "--max-batch-size" && i + 1 < argc) {
            try {
                config.max_batch_size = std::stoi(argv[++i]);
            } catch (const std::exception& e) {
                LOG_ERROR("Invalid max batch size: " + std::string(argv[i]));
                throw std::runtime_error("Invalid max batch size");
            }
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
    LOG_INFO("  Threads: " + std::to_string(config.thread_pool_size));
    LOG_INFO("  Use XDG: " + std::string(config.use_xdg ? "yes" : "no"));
    LOG_INFO("  Rate Limiter: " + std::string(config.enable_rate_limiter ? "enabled" : "disabled"));
    if (config.enable_rate_limiter) {
        LOG_INFO("  Max Requests/Min: " + std::to_string(config.max_requests_per_minute));
    }
    LOG_INFO("  Max Batch Size: " + std::to_string(config.max_batch_size));

    return config;
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
        } else if (key == "threads") {
            config.thread_pool_size = std::stoi(value);
        } else if (key == "enable_rate_limiter") {
            config.enable_rate_limiter = (value == "true" || value == "1");
        } else if (key == "max_requests_per_minute") {
            config.max_requests_per_minute = std::stoi(value);
        } else if (key == "max_batch_size") {
            config.max_batch_size = std::stoi(value);
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
    file << "threads = " << config.thread_pool_size << "\n";
    file << "enable_rate_limiter = " << (config.enable_rate_limiter ? "true" : "false") << "\n";
    file << "max_requests_per_minute = " << config.max_requests_per_minute << "\n";
    file << "max_batch_size = " << config.max_batch_size << "\n";

    file.close();
    LOG_INFO("Configuration saved to: " + config_file.string());
    return true;
}

void ConfigParser::print_help(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n"
              << "IP Geolocation & AS Lookup Service\n\n"
              << "Options:\n"
              << "  --config <path>      Path to configuration file\n"
              << "  --city-db <path>     Path to City MaxMind database\n"
              << "                      (default: ~/.local/share/ip-server/databases/GeoLite2-City.mmdb)\n"
              << "  --asn-db <path>      Path to ASN MaxMind database\n"
              << "                      (default: ~/.local/share/ip-server/databases/GeoLite2-ASN.mmdb)\n"
              << "  --host <address>     Server host address\n"
              << "                      (default: 0.0.0.0)\n"
              << "  --port <port>        Server port\n"
              << "                      (default: 8080)\n"
              << "  --threads <count>    Thread pool size\n"
              << "                      (default: 4)\n"
              << "  --enable-rate-limiter <true|false>\n"
              << "                      Enable rate limiting\n"
              << "                      (default: true)\n"
              << "  --max-requests-per-minute <count>\n"
              << "                      Maximum requests per IP per minute\n"
              << "                      (default: 100)\n"
              << "  --max-batch-size <count>\n"
              << "                      Maximum batch size for batch lookup\n"
              << "                      (default: 100)\n"
              << "  --no-xdg             Disable XDG directory standard\n"
              << "  --help, -h           Show this help message\n\n"
              << "XDG Directories:\n"
              << "  Config:  $XDG_CONFIG_HOME/ip-server/ (default: ~/.config/ip-server/)\n"
              << "  Data:    $XDG_DATA_HOME/ip-server/ (default: ~/.local/share/ip-server/)\n"
              << "  Cache:   $XDG_CACHE_HOME/ip-server/ (default: ~/.cache/ip-server/)\n\n"
              << "Environment Variables:\n"
              << "  XDG_CONFIG_HOME  Configuration directory\n"
              << "  XDG_DATA_HOME    Data directory\n"
              << "  XDG_CACHE_HOME   Cache directory\n\n"
              << "Examples:\n"
              << "  " << program_name << "\n"
              << "  " << program_name << " --port 9000\n"
              << "  " << program_name << " --config /path/to/config.toml\n"
              << "  " << program_name << " --city-db /path/to/GeoLite2-City.mmdb --asn-db /path/to/GeoLite2-ASN.mmdb\n"
              << "  " << program_name << " --host 127.0.0.1 --port 8080\n\n"
              << "API Endpoints:\n"
              << "  GET  /                       - Service information\n"
              << "  GET  /health                 - Health check\n"
              << "  GET  /lookup?ip=<address>     - Single IP lookup\n"
              << "  POST /lookup                 - Batch lookup\n"
              << "                                Body: {\"ips\": [\"1.1.1.1\", \"8.8.8.8\"]}\n\n"
              << "For more information, visit: https://dev.maxmind.com/geoip/geolite2-free-geolocation-data\n";
}

} // namespace ip_server