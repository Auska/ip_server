#include "config.h"

#include <array>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string_view>

#include "logger.h"
#include "xdg.h"

namespace ip_server {

namespace {

bool parse_bool_string(const std::string& value) {
    return value == "true" || value == "1";
}

void apply_xdg_defaults(ServerConfig& config) {
    config.city_db_path  = ip_server::XDGPaths::city_db_path().string();
    config.asn_db_path   = ip_server::XDGPaths::asn_db_path().string();
    config.oui_db_path   = ip_server::XDGPaths::oui_db_path().string();
    config.config_file   = ip_server::XDGPaths::config_file();
    config.log_file_path = ip_server::XDGPaths::log_file_path().string();
}

}  // namespace

ServerConfig ConfigParser::default_config() {
    ServerConfig config;
    config.host             = "0.0.0.0";
    config.port             = 8080;
    config.thread_pool_size = 4;
    return config;
}

cxxopts::Options ConfigParser::create_option_parser() {
    cxxopts::Options options("ip_server", "IP Geolocation & AS Lookup Service");

    options.add_options()(
        "help,h", "Show this help message")(
        "config", "Path to configuration file",
        cxxopts::value<std::string>())(
        "city-db", "Path to City MaxMind database",
        cxxopts::value<std::string>())(
        "asn-db", "Path to ASN MaxMind database",
        cxxopts::value<std::string>())(
        "oui-db", "Path to OUI database",
        cxxopts::value<std::string>())(
        "host", "Server host address",
        cxxopts::value<std::string>()->default_value("0.0.0.0"))(
        "port", "Server port",
        cxxopts::value<uint16_t>()->default_value("8080"))(
        "threads", "Thread pool size",
        cxxopts::value<int>()->default_value("4"))(
        "cache-size", "Cache size",
        cxxopts::value<size_t>()->default_value("10000"))(
        "enable-rate-limiter", "Enable rate limiting",
        cxxopts::value<std::string>()->default_value("true"))(
        "max-requests-per-minute", "Maximum requests per IP per minute",
        cxxopts::value<int>()->default_value("100"))(
        "max-batch-size", "Maximum batch size for batch lookup",
        cxxopts::value<int>()->default_value("100"))(
        "enable-api-auth", "Enable API authentication",
        cxxopts::value<std::string>()->default_value("false"))(
        "api-keys-file", "Path to file containing API keys",
        cxxopts::value<std::string>())(
        "default-api-key", "Default API key for testing",
        cxxopts::value<std::string>())(
        "enable-file-logging", "Enable file logging",
        cxxopts::value<std::string>()->default_value("false"))(
        "log-file", "Path to log file",
        cxxopts::value<std::string>()->default_value("logs/ip_server.log"))(
        "log-enable-stdout", "Enable stdout logging",
        cxxopts::value<std::string>()->default_value("true"))(
        "log-rotation", "Log rotation type: none, size, time, both",
        cxxopts::value<std::string>()->default_value("size"))(
        "log-max-size", "Maximum log file size in MB",
        cxxopts::value<size_t>()->default_value("10"))(
        "log-rotation-interval", "Time interval in minutes for time-based rotation",
        cxxopts::value<int>()->default_value("1440"))(
        "log-max-backups", "Maximum number of backup log files",
        cxxopts::value<int>()->default_value("5"))(
        "log-level", "Log level: trace, debug, info, warn, error, critical, off",
        cxxopts::value<std::string>()->default_value("info"));

    return options;
}

ServerConfig ConfigParser::handle_config_file(const std::filesystem::path& config_file) {
    std::ifstream check(config_file);
    bool const is_empty = check.peek() == std::ifstream::traits_type::eof();
    check.close();

    if (is_empty) {
        LOG_INFO("Config file is empty, creating default configuration: " + config_file.string());
        ServerConfig cfg = default_config();
        apply_xdg_defaults(cfg);
        cfg.config_file = config_file;

        if (save_to_file(cfg, config_file)) {
            LOG_INFO("Created default configuration file: " + config_file.string());
            return cfg;
        }
        LOG_WARNING("Failed to create default config file, using built-in defaults");
        return default_config();
    }

    try {
        auto cfg = load_from_file(config_file);
        LOG_INFO("Loaded configuration from: " + config_file.string());
        return cfg;
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to load config file: " + std::string(e.what()));
        return default_config();
    }
}

ServerConfig ConfigParser::parse(int argc, char* argv[]) {
    ServerConfig config = default_config();

    try {
        auto options = create_option_parser();
        auto result = options.parse(argc, argv);

        if (result.contains("help")) {
            std::cout << options.help() << '\n';
            std::exit(0);
        }

        apply_xdg_defaults(config);

        LOG_INFO("Using XDG paths:");
        LOG_INFO("  Config: " + config.config_file.string());
        LOG_INFO("  City DB: " + config.city_db_path);
        LOG_INFO("  ASN DB: " + config.asn_db_path);
        LOG_INFO("  OUI DB: " + config.oui_db_path);
        LOG_INFO("  Log File: " + config.log_file_path);

        if (result.contains("config")) {
            config.config_file = result["config"].as<std::string>();
        }

        if (!config.config_file.empty() && std::filesystem::exists(config.config_file)) {
            config = handle_config_file(config.config_file);
        }

        apply_cli_overrides(result, config);

    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing options: " << e.what() << '\n';
        throw std::runtime_error(std::string("Failed to parse command line options: ") + e.what());
    }

    log_config(config);
    validate(config);

    return config;
}

void ConfigParser::validate(const ServerConfig& config) {
    using namespace config_limits;

    // Validate port range
    if (config.port < MIN_PORT) {
        LOG_ERROR("Port must be between " + std::to_string(MIN_PORT) + " and "
                  + std::to_string(MAX_PORT) + ", got: " + std::to_string(config.port));
        throw std::runtime_error("Invalid port number: must be between "
                                 + std::to_string(MIN_PORT) + " and " + std::to_string(MAX_PORT));
    }

    // Validate thread pool size
    if (config.thread_pool_size < MIN_THREAD_POOL || config.thread_pool_size > MAX_THREAD_POOL) {
        LOG_ERROR("Thread pool size must be between " + std::to_string(MIN_THREAD_POOL)
                  + " and " + std::to_string(MAX_THREAD_POOL) + ", got: "
                  + std::to_string(config.thread_pool_size));
        throw std::runtime_error("Invalid thread pool size: must be between "
                                 + std::to_string(MIN_THREAD_POOL) + " and "
                                 + std::to_string(MAX_THREAD_POOL));
    }

    // Validate cache size
    if (config.cache_size > MAX_CACHE_SIZE) {
        LOG_ERROR("Cache size must be between 0 and " + std::to_string(MAX_CACHE_SIZE) + ", got: "
                  + std::to_string(config.cache_size));
        throw std::runtime_error("Invalid cache size: must be between 0 and "
                                 + std::to_string(MAX_CACHE_SIZE));
    }

    // Validate rate limiter settings
    if (config.enable_rate_limiter) {
        if (config.max_requests_per_minute < MIN_RATE_LIMIT
            || config.max_requests_per_minute > MAX_RATE_LIMIT) {
            LOG_ERROR("Max requests per minute must be between " + std::to_string(MIN_RATE_LIMIT)
                      + " and " + std::to_string(MAX_RATE_LIMIT) + ", got: "
                      + std::to_string(config.max_requests_per_minute));
            throw std::runtime_error("Invalid max requests per minute: must be between "
                                     + std::to_string(MIN_RATE_LIMIT) + " and "
                                     + std::to_string(MAX_RATE_LIMIT));
        }
    }

    // Validate batch size limit
    if (config.max_batch_size < MIN_BATCH_SIZE || config.max_batch_size > MAX_BATCH_SIZE) {
        LOG_ERROR("Max batch size must be between " + std::to_string(MIN_BATCH_SIZE)
                  + " and " + std::to_string(MAX_BATCH_SIZE) + ", got: "
                  + std::to_string(config.max_batch_size));
        throw std::runtime_error("Invalid max batch size: must be between "
                                 + std::to_string(MIN_BATCH_SIZE) + " and "
                                 + std::to_string(MAX_BATCH_SIZE));
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
        static constexpr auto valid_rotation_types =
            std::to_array<std::string_view>({"none", "size", "time", "both"});
        bool valid_rotation = false;
        for (const auto& t : valid_rotation_types) {
            if (config.log_rotation_type == t) {
                valid_rotation = true;
                break;
            }
        }
        if (!valid_rotation) {
            LOG_ERROR("Invalid log rotation type: " + config.log_rotation_type);
            throw std::runtime_error(
                "Invalid log rotation type: must be none, size, time, or both");
        }

        if (config.log_max_file_size < MIN_LOG_FILE_SIZE
            || config.log_max_file_size > MAX_LOG_FILE_SIZE) {
            LOG_ERROR("Log max file size must be between "
                      + std::to_string(MIN_LOG_FILE_SIZE / (1024 * 1024)) + " MB and "
                      + std::to_string(MAX_LOG_FILE_SIZE / (1024 * 1024)) + " GB, got: "
                      + std::to_string(config.log_max_file_size / (1024 * 1024)) + " MB");
            throw std::runtime_error("Invalid log max file size");
        }

        if (config.log_rotation_interval_minutes < MIN_ROTATION_INTERVAL
            || config.log_rotation_interval_minutes > MAX_ROTATION_INTERVAL) {
            LOG_ERROR("Log rotation interval must be between "
                      + std::to_string(MIN_ROTATION_INTERVAL) + " and "
                      + std::to_string(MAX_ROTATION_INTERVAL) + " minutes, got: "
                      + std::to_string(config.log_rotation_interval_minutes));
            throw std::runtime_error("Invalid log rotation interval");
        }

        if (config.log_max_backup_files < MIN_BACKUP_FILES
            || config.log_max_backup_files > MAX_BACKUP_FILES) {
            LOG_ERROR("Log max backup files must be between "
                      + std::to_string(MIN_BACKUP_FILES) + " and "
                      + std::to_string(MAX_BACKUP_FILES) + ", got: "
                      + std::to_string(config.log_max_backup_files));
            throw std::runtime_error("Invalid log max backup files");
        }
    }

    LOG_INFO("Configuration validation passed");
}

void ConfigParser::from_json(ServerConfig& config, const nlohmann::json& j) {
    auto read_str = [&](const char* key, std::string& field) {
        if (j.contains(key)) field = j[key].get<std::string>();
    };
    auto read_int = [&](const char* key, auto& field) {
        if (j.contains(key)) {
            if (j[key].is_number_integer()) field = j[key].get<std::decay_t<decltype(field)>>();
            else if (j[key].is_string()) field = static_cast<std::decay_t<decltype(field)>>(std::stoll(j[key].get<std::string>()));
        }
    };
    auto read_bool = [&](const char* key, bool& field) {
        if (j.contains(key)) {
            if (j[key].is_boolean()) field = j[key].get<bool>();
            else if (j[key].is_string()) field = (j[key].get<std::string>() == "true" || j[key].get<std::string>() == "1");
        }
    };

    read_str("host", config.host);
    read_int("port", config.port);
    read_str("city_db", config.city_db_path);
    read_str("asn_db", config.asn_db_path);
    read_str("oui_db", config.oui_db_path);
    read_int("threads", config.thread_pool_size);
    read_int("cache_size", config.cache_size);
    read_bool("enable_rate_limiter", config.enable_rate_limiter);
    read_int("max_requests_per_minute", config.max_requests_per_minute);
    read_int("max_batch_size", config.max_batch_size);
    read_bool("enable_api_auth", config.enable_api_auth);
    read_str("api_keys_file", config.api_keys_file);
    read_str("default_api_key", config.default_api_key);
    read_bool("enable_file_logging", config.enable_file_logging);
    read_str("log_file", config.log_file_path);
    read_bool("log_enable_stdout", config.log_enable_stdout);
    read_str("log_rotation", config.log_rotation_type);
    read_str("log_level", config.log_level);

    if (j.contains("log_max_file_size")) {
        if (j["log_max_file_size"].is_number_integer())
            config.log_max_file_size = j["log_max_file_size"].get<size_t>() * 1024 * 1024;
        else if (j["log_max_file_size"].is_string())
            config.log_max_file_size = std::stoull(j["log_max_file_size"].get<std::string>()) * 1024 * 1024;
    }
    if (j.contains("log_rotation_interval_minutes")) {
        if (j["log_rotation_interval_minutes"].is_number_integer())
            config.log_rotation_interval_minutes = j["log_rotation_interval_minutes"].get<int>();
        else if (j["log_rotation_interval_minutes"].is_string())
            config.log_rotation_interval_minutes = std::stoi(j["log_rotation_interval_minutes"].get<std::string>());
    }
    if (j.contains("log_max_backup_files")) {
        if (j["log_max_backup_files"].is_number_integer())
            config.log_max_backup_files = j["log_max_backup_files"].get<int>();
        else if (j["log_max_backup_files"].is_string())
            config.log_max_backup_files = std::stoi(j["log_max_backup_files"].get<std::string>());
    }
}

ServerConfig ConfigParser::load_from_file(const std::filesystem::path& config_file) {
    ServerConfig config = default_config();

    std::ifstream file(config_file);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_file.string());
    }

    nlohmann::json j;
    file >> j;
    from_json(config, j);

    LOG_INFO("Loaded JSON configuration from: " + config_file.string());
    return config;
}

void ConfigParser::apply_cli_overrides(const cxxopts::ParseResult& result, ServerConfig& config) {
    auto set_str = [&](const char* key, std::string& f) { if (result.contains(key)) f = result[key].as<std::string>(); };
    auto set_int = [&](const char* key, auto& f) { if (result.contains(key)) f = result[key].as<std::decay_t<decltype(f)>>(); };

    set_str("host", config.host);
    set_int("port", config.port);
    set_int("threads", config.thread_pool_size);
    set_int("cache-size", config.cache_size);
    set_str("city-db", config.city_db_path);
    set_str("asn-db", config.asn_db_path);
    set_str("oui-db", config.oui_db_path);
    if (result.contains("enable-rate-limiter")) config.enable_rate_limiter = parse_bool_string(result["enable-rate-limiter"].as<std::string>());
    set_int("max-requests-per-minute", config.max_requests_per_minute);
    set_int("max-batch-size", config.max_batch_size);
    if (result.contains("enable-api-auth")) config.enable_api_auth = parse_bool_string(result["enable-api-auth"].as<std::string>());
    set_str("api-keys-file", config.api_keys_file);
    set_str("default-api-key", config.default_api_key);

    if (result.contains("enable-file-logging")) {
        config.enable_file_logging = parse_bool_string(result["enable-file-logging"].as<std::string>());
        if (config.enable_file_logging && config.log_file_path == "logs/ip_server.log")
            config.log_file_path = ip_server::XDGPaths::log_file_path().string();
    }
    if (result.contains("log-file")) { config.log_file_path = result["log-file"].as<std::string>(); config.enable_file_logging = true; }
    if (result.contains("log-enable-stdout")) config.log_enable_stdout = parse_bool_string(result["log-enable-stdout"].as<std::string>());
    set_str("log-rotation", config.log_rotation_type);
    if (result.contains("log-max-size")) config.log_max_file_size = result["log-max-size"].as<size_t>() * 1024 * 1024;
    set_int("log-rotation-interval", config.log_rotation_interval_minutes);
    set_int("log-max-backups", config.log_max_backup_files);
    set_str("log-level", config.log_level);
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
        j["log_max_file_size"]   = config.log_max_file_size / (static_cast<size_t>(1024 * 1024));  // Convert to MB
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

void ConfigParser::log_config(const ServerConfig& config) {
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
            LOG_INFO("  Max File Size: " + std::to_string(config.log_max_file_size / static_cast<size_t>(1024 * 1024)) + " MB");
        }
        if (config.log_rotation_type == "time" || config.log_rotation_type == "both") {
            LOG_INFO("  Rotation Interval: " + std::to_string(config.log_rotation_interval_minutes) + " minutes");
        }
        LOG_INFO("  Max Backup Files: " + std::to_string(config.log_max_backup_files));
    }
    LOG_INFO("  Stdout Logging: " + std::string(config.log_enable_stdout ? "enabled" : "disabled"));
    LOG_INFO("  Log Level: " + config.log_level);
}

}  // namespace ip_server