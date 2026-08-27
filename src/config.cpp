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

bool parseBoolString(const std::string& value) {
    return value == "true" || value == "1";
}

void applyXdgDefaults(ServerConfig& config) {
    config.city_db_path_  = ip_server::XDGPaths::city_db_path().string();
    config.asn_db_path_   = ip_server::XDGPaths::asn_db_path().string();
    config.oui_db_path_   = ip_server::XDGPaths::oui_db_path().string();
    config.config_file_   = ip_server::XDGPaths::config_file();
    config.log_file_path_ = ip_server::XDGPaths::log_file_path().string();
}

}  // namespace

cxxopts::Options ConfigParser::create_option_parser() {
    cxxopts::Options options("ip_server", "IP Geolocation & AS Lookup Service");

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
                                               cxxopts::value<size_t>()->default_value("10000"))(
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
                                                     cxxopts::value<std::string>()->default_value(
                                                         "logs/ip_server.log"))(
        "log-enable-stdout", "Enable stdout logging",
        cxxopts::value<std::string>()->default_value(
            "true"))("log-rotation", "Log rotation type: none or size",
                     cxxopts::value<std::string>()->default_value(
                         "size"))("log-max-size", "Maximum log file size in MB",
                                  cxxopts::value<size_t>()->default_value(
                                      "10"))("log-max-backups", "Maximum number of backup log files",
        cxxopts::value<int>()->default_value(
            "5"))("log-level", "Log level: debug, info, warn, error",
                  cxxopts::value<std::string>()->default_value("info"));

    return options;
}

ServerConfig ConfigParser::handle_config_file(const std::filesystem::path& config_file) {
    std::ifstream check(config_file);
    bool const is_empty = check.peek() == std::ifstream::traits_type::eof();
    check.close();

    if (is_empty) {
        LOG_INFO("Config file is empty, creating default configuration: " + config_file.string());
        ServerConfig cfg;
        applyXdgDefaults(cfg);
        cfg.config_file_ = config_file;

        if (save_to_file(cfg, config_file)) {
            LOG_INFO("Created default configuration file: " + config_file.string());
            return cfg;
        }
        LOG_WARNING("Failed to create default config file, using built-in defaults");
        return ServerConfig{};
    }

    try {
        auto cfg = load_from_file(config_file);
        LOG_INFO("Loaded configuration from: " + config_file.string());
        return cfg;
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to load config file: " + std::string(e.what()));
        return ServerConfig{};
    }
}

ServerConfig ConfigParser::parse(int argc, char* argv[]) {
    ServerConfig config;

    try {
        auto options = create_option_parser();
        auto result  = options.parse(argc, argv);

        if (result.contains("help")) {
            std::cout << options.help() << '\n';
            std::exit(0);
        }

        applyXdgDefaults(config);

        LOG_INFO("Using XDG paths:");
        LOG_INFO("  Config: " + config.config_file_.string());
        LOG_INFO("  City DB: " + config.city_db_path_);
        LOG_INFO("  ASN DB: " + config.asn_db_path_);
        LOG_INFO("  OUI DB: " + config.oui_db_path_);
        LOG_INFO("  Log File: " + config.log_file_path_);

        if (result.contains("config")) {
            config.config_file_ = result["config"].as<std::string>();
        }

        if (!config.config_file_.empty() && std::filesystem::exists(config.config_file_)) {
            config = handle_config_file(config.config_file_);
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
    if (config.port_ < MIN_PORT) {
        LOG_ERROR("Port must be between " + std::to_string(MIN_PORT) + " and "
                  + std::to_string(MAX_PORT) + ", got: " + std::to_string(config.port_));
        throw std::runtime_error("Invalid port number: must be between " + std::to_string(MIN_PORT)
                                 + " and " + std::to_string(MAX_PORT));
    }

    // Validate thread pool size
    if (config.thread_pool_size_ < MIN_THREAD_POOL || config.thread_pool_size_ > MAX_THREAD_POOL) {
        LOG_ERROR("Thread pool size must be between " + std::to_string(MIN_THREAD_POOL) + " and "
                  + std::to_string(MAX_THREAD_POOL)
                  + ", got: " + std::to_string(config.thread_pool_size_));
        throw std::runtime_error("Invalid thread pool size: must be between "
                                 + std::to_string(MIN_THREAD_POOL) + " and "
                                 + std::to_string(MAX_THREAD_POOL));
    }

    // Validate cache size
    if (config.cache_size_ > MAX_CACHE_SIZE) {
        LOG_ERROR("Cache size must be between 0 and " + std::to_string(MAX_CACHE_SIZE)
                  + ", got: " + std::to_string(config.cache_size_));
        throw std::runtime_error("Invalid cache size: must be between 0 and "
                                 + std::to_string(MAX_CACHE_SIZE));
    }

    // Validate rate limiter settings
    if (config.enable_rate_limiter_) {
        if (config.max_requests_per_minute_ < MIN_RATE_LIMIT
            || config.max_requests_per_minute_ > MAX_RATE_LIMIT) {
            LOG_ERROR("Max requests per minute must be between " + std::to_string(MIN_RATE_LIMIT)
                      + " and " + std::to_string(MAX_RATE_LIMIT)
                      + ", got: " + std::to_string(config.max_requests_per_minute_));
            throw std::runtime_error("Invalid max requests per minute: must be between "
                                     + std::to_string(MIN_RATE_LIMIT) + " and "
                                     + std::to_string(MAX_RATE_LIMIT));
        }
    }

    // Validate batch size limit
    if (config.max_batch_size_ < MIN_BATCH_SIZE || config.max_batch_size_ > MAX_BATCH_SIZE) {
        LOG_ERROR("Max batch size must be between " + std::to_string(MIN_BATCH_SIZE) + " and "
                  + std::to_string(MAX_BATCH_SIZE)
                  + ", got: " + std::to_string(config.max_batch_size_));
        throw std::runtime_error("Invalid max batch size: must be between "
                                 + std::to_string(MIN_BATCH_SIZE) + " and "
                                 + std::to_string(MAX_BATCH_SIZE));
    }

    // Validate database paths
    for (const auto& [name, path] :
         {std::make_pair("City", config.city_db_path_), std::make_pair("ASN", config.asn_db_path_),
          std::make_pair("OUI", config.oui_db_path_)}) {
        if (!path.empty() && !std::filesystem::exists(path)) {
            LOG_WARNING(name + std::string(" database file does not exist: ") + path);
        }
    }

    // Validate logging configuration
    if (config.enable_file_logging_) {
        static constexpr auto valid_rotation_types =
            std::to_array<std::string_view>({"none", "size"});
        bool valid_rotation = false;
        for (const auto& t : valid_rotation_types) {
            if (config.log_rotation_type_ == t) {
                valid_rotation = true;
                break;
            }
        }
        if (!valid_rotation) {
            LOG_ERROR("Invalid log rotation type: " + config.log_rotation_type_);
            throw std::runtime_error("Invalid log rotation type: must be none or size");
        }

        if (config.log_max_file_size_ < MIN_LOG_FILE_SIZE
            || config.log_max_file_size_ > MAX_LOG_FILE_SIZE) {
            LOG_ERROR("Log max file size must be between "
                      + std::to_string(MIN_LOG_FILE_SIZE / (1024 * 1024)) + " MB and "
                      + std::to_string(MAX_LOG_FILE_SIZE / (1024 * 1024)) + " GB, got: "
                      + std::to_string(config.log_max_file_size_ / (1024 * 1024)) + " MB");
            throw std::runtime_error("Invalid log max file size");
        }

        if (config.log_max_backup_files_ < MIN_BACKUP_FILES
            || config.log_max_backup_files_ > MAX_BACKUP_FILES) {
            LOG_ERROR("Log max backup files must be between " + std::to_string(MIN_BACKUP_FILES)
                      + " and " + std::to_string(MAX_BACKUP_FILES)
                      + ", got: " + std::to_string(config.log_max_backup_files_));
            throw std::runtime_error("Invalid log max backup files");
        }
    }

    LOG_INFO("Configuration validation passed");
}

void ConfigParser::from_json(ServerConfig& config, const nlohmann::json& j) {
    auto readStr = [&](const char* key, std::string& field) {
        if (j.contains(key)) field = j[key].get<std::string>();
    };
    auto readInt = [&](const char* key, auto& field) {
        if (j.contains(key)) {
            if (j[key].is_number_integer())
                field = j[key].get<std::decay_t<decltype(field)>>();
            else if (j[key].is_string())
                field = static_cast<std::decay_t<decltype(field)>>(
                    std::stoll(j[key].get<std::string>()));
        }
    };
    auto readBool = [&](const char* key, bool& field) {
        if (j.contains(key)) {
            if (j[key].is_boolean())
                field = j[key].get<bool>();
            else if (j[key].is_string())
                field = (j[key].get<std::string>() == "true" || j[key].get<std::string>() == "1");
        }
    };

    readStr("host", config.host_);
    readInt("port", config.port_);
    readStr("city_db", config.city_db_path_);
    readStr("asn_db", config.asn_db_path_);
    readStr("oui_db", config.oui_db_path_);
    readInt("threads", config.thread_pool_size_);
    readInt("cache_size", config.cache_size_);
    readBool("enable_rate_limiter", config.enable_rate_limiter_);
    readInt("max_requests_per_minute", config.max_requests_per_minute_);
    readInt("max_batch_size", config.max_batch_size_);
    readBool("enable_api_auth", config.enable_api_auth_);
    readStr("api_keys_file", config.api_keys_file_);
    readStr("default_api_key", config.default_api_key_);
    readBool("enable_file_logging", config.enable_file_logging_);
    readStr("log_file", config.log_file_path_);
    readBool("log_enable_stdout", config.log_enable_stdout_);
    readStr("log_rotation", config.log_rotation_type_);
    readStr("log_level", config.log_level_);

    if (j.contains("log_max_file_size")) {
        size_t raw = 0;
        if (j["log_max_file_size"].is_number_integer())
            raw = j["log_max_file_size"].get<size_t>();
        else if (j["log_max_file_size"].is_string())
            raw = std::stoull(j["log_max_file_size"].get<std::string>());
        config.log_max_file_size_ = raw * 1024 * 1024;
    }
    readInt("log_max_backup_files", config.log_max_backup_files_);
}

ServerConfig ConfigParser::load_from_file(const std::filesystem::path& config_file) {
    ServerConfig config;

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
    auto set_str = [&](const char* key, std::string& f) {
        if (result.contains(key)) f = result[key].as<std::string>();
    };
    auto set_int = [&](const char* key, auto& f) {
        if (result.contains(key)) f = result[key].as<std::decay_t<decltype(f)>>();
    };

    set_str("host", config.host_);
    set_int("port", config.port_);
    set_int("threads", config.thread_pool_size_);
    set_int("cache-size", config.cache_size_);
    set_str("city-db", config.city_db_path_);
    set_str("asn-db", config.asn_db_path_);
    set_str("oui-db", config.oui_db_path_);
    if (result.contains("enable-rate-limiter"))
        config.enable_rate_limiter_ =
            parseBoolString(result["enable-rate-limiter"].as<std::string>());
    set_int("max-requests-per-minute", config.max_requests_per_minute_);
    set_int("max-batch-size", config.max_batch_size_);
    if (result.contains("enable-api-auth"))
        config.enable_api_auth_ = parseBoolString(result["enable-api-auth"].as<std::string>());
    set_str("api-keys-file", config.api_keys_file_);
    set_str("default-api-key", config.default_api_key_);

    if (result.contains("enable-file-logging")) {
        config.enable_file_logging_ =
            parseBoolString(result["enable-file-logging"].as<std::string>());
        if (config.enable_file_logging_ && config.log_file_path_ == "logs/ip_server.log")
            config.log_file_path_ = ip_server::XDGPaths::log_file_path().string();
    }
    if (result.contains("log-file")) {
        config.log_file_path_       = result["log-file"].as<std::string>();
        config.enable_file_logging_ = true;
    }
    if (result.contains("log-enable-stdout"))
        config.log_enable_stdout_ = parseBoolString(result["log-enable-stdout"].as<std::string>());
    set_str("log-rotation", config.log_rotation_type_);
    if (result.contains("log-max-size"))
        config.log_max_file_size_ = result["log-max-size"].as<size_t>() * 1024 * 1024;
    set_int("log-max-backups", config.log_max_backup_files_);
    set_str("log-level", config.log_level_);
}

bool ConfigParser::save_to_file(const ServerConfig& config,
                                const std::filesystem::path& config_file) {
    try {
        nlohmann::json j;

        // Build JSON object
        j["host"]                    = config.host_;
        j["port"]                    = config.port_;
        j["city_db"]                 = config.city_db_path_;
        j["asn_db"]                  = config.asn_db_path_;
        j["oui_db"]                  = config.oui_db_path_;
        j["threads"]                 = config.thread_pool_size_;
        j["cache_size"]              = config.cache_size_;
        j["enable_rate_limiter"]     = config.enable_rate_limiter_;
        j["max_requests_per_minute"] = config.max_requests_per_minute_;
        j["max_batch_size"]          = config.max_batch_size_;
        j["enable_api_auth"]         = config.enable_api_auth_;
        j["api_keys_file"]           = config.api_keys_file_;
        if (!config.default_api_key_.empty()) {
            j["default_api_key"] = config.default_api_key_;
        }
        j["enable_file_logging"] = config.enable_file_logging_;
        j["log_file"]            = config.log_file_path_;
        j["log_enable_stdout"]   = config.log_enable_stdout_;
        j["log_rotation"]        = config.log_rotation_type_;
        j["log_max_file_size"] =
            config.log_max_file_size_ / (static_cast<size_t>(1024 * 1024));  // Convert to MB
        j["log_max_backup_files"] = config.log_max_backup_files_;
        j["log_level"]                     = config.log_level_;

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
    LOG_INFO("  Host: " + config.host_);
    LOG_INFO("  Port: " + std::to_string(config.port_));
    LOG_INFO("  City DB: " + config.city_db_path_);
    LOG_INFO("  ASN DB: " + config.asn_db_path_);
    LOG_INFO("  OUI DB: " + config.oui_db_path_);
    LOG_INFO("  Threads: " + std::to_string(config.thread_pool_size_));
    LOG_INFO("  Rate Limiter: "
             + std::string(config.enable_rate_limiter_ ? "enabled" : "disabled"));
    if (config.enable_rate_limiter_) {
        LOG_INFO("  Max Requests/Min: " + std::to_string(config.max_requests_per_minute_));
    }
    LOG_INFO("  Max Batch Size: " + std::to_string(config.max_batch_size_));
    LOG_INFO("  API Auth: " + std::string(config.enable_api_auth_ ? "enabled" : "disabled"));
    if (config.enable_api_auth_) {
        LOG_INFO("  API Keys File: " + config.api_keys_file_);
        if (!config.default_api_key_.empty()) {
            LOG_INFO("  Default API Key: " + config.default_api_key_.substr(0, 8) + "...");
        }
    }
    LOG_INFO("  File Logging: "
             + std::string(config.enable_file_logging_ ? "enabled" : "disabled"));
    if (config.enable_file_logging_) {
        LOG_INFO("  Log File: " + config.log_file_path_);
        LOG_INFO("  Log Rotation: " + config.log_rotation_type_);
        if (config.log_rotation_type_ == "size") {
            LOG_INFO("  Max File Size: "
                     + std::to_string(config.log_max_file_size_ / static_cast<size_t>(1024 * 1024))
                     + " MB");
        }
        LOG_INFO("  Max Backup Files: " + std::to_string(config.log_max_backup_files_));
    }
    LOG_INFO("  Stdout Logging: "
             + std::string(config.log_enable_stdout_ ? "enabled" : "disabled"));
    LOG_INFO("  Log Level: " + config.log_level_);
}

}  // namespace ip_server