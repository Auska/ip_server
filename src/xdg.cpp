#include "xdg.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>

#include "logger.h"

namespace ip_server {

XDGPaths& XDGPaths::instance() {
    static XDGPaths instance;
    return instance;
}

XDGPaths::XDGPaths() {
    LOG_INFO("XDG paths initialized");
}

std::string XDGPaths::get_env(const char* name, const std::string& default_value) {
    const char* value = std::getenv(name);
    return (value != nullptr) ? value : default_value;
}

std::filesystem::path XDGPaths::get_home() {
    const char* home = std::getenv("HOME");
    if (home != nullptr) {
        return home;
    }

    // Fallback to getpwuid
    struct passwd const* pw = getpwuid(getuid());
    if (pw != nullptr) {
        return pw->pw_dir;
    }

    // Last resort
    return "/tmp";
}

std::filesystem::path XDGPaths::config_home() {
    std::string path = get_env("XDG_CONFIG_HOME", "");
    if (!path.empty()) {
        return path;
    }
    return get_home() / ".config";
}

std::filesystem::path XDGPaths::data_home() {
    std::string path = get_env("XDG_DATA_HOME", "");
    if (!path.empty()) {
        return path;
    }
    return get_home() / ".local" / "share";
}

std::filesystem::path XDGPaths::cache_home() {
    std::string path = get_env("XDG_CACHE_HOME", "");
    if (!path.empty()) {
        return path;
    }
    return get_home() / ".cache";
}

std::filesystem::path XDGPaths::state_home() {
    std::string path = get_env("XDG_STATE_HOME", "");
    if (!path.empty()) {
        return path;
    }
    return get_home() / ".local" / "state";
}

std::filesystem::path XDGPaths::app_config_dir() {
    return config_home() / APP_NAME;
}

std::filesystem::path XDGPaths::app_data_dir() {
    return data_home() / APP_NAME;
}

std::filesystem::path XDGPaths::app_cache_dir() {
    return cache_home() / APP_NAME;
}

std::filesystem::path XDGPaths::app_state_dir() {
    return state_home() / APP_NAME;
}

std::filesystem::path XDGPaths::config_file() {
    return app_config_dir() / "config.json";
}

std::filesystem::path XDGPaths::database_dir() {
    return app_data_dir() / "databases";
}

std::filesystem::path XDGPaths::city_db_path() const {
    return database_dir() / "GeoLite2-City.mmdb";
}

std::filesystem::path XDGPaths::asn_db_path() const {
    return database_dir() / "GeoLite2-ASN.mmdb";
}

std::filesystem::path XDGPaths::oui_db_path() const {
    return database_dir() / "master_oui.db";
}

std::filesystem::path XDGPaths::log_file_path() {
    // Use XDG state home for logs (or data home as fallback)
    std::filesystem::path log_dir;
    std::string const state_path = get_env("XDG_STATE_HOME", "");
    if (!state_path.empty()) {
        log_dir = std::filesystem::path(state_path) / APP_NAME / "logs";
    } else {
        // Fallback to data home
        log_dir = data_home() / APP_NAME / "logs";
    }
    return log_dir / "ip_server.log";
}

void XDGPaths::ensure_directories() const {
    std::error_code ec;

    std::filesystem::create_directories(app_config_dir(), ec);
    if (ec) {
        LOG_WARNING("Failed to create config directory: " + ec.message());
    }

    std::filesystem::create_directories(app_data_dir(), ec);
    if (ec) {
        LOG_WARNING("Failed to create data directory: " + ec.message());
    }

    std::filesystem::create_directories(app_cache_dir(), ec);
    if (ec) {
        LOG_WARNING("Failed to create cache directory: " + ec.message());
    }

    std::filesystem::create_directories(database_dir(), ec);
    if (ec) {
        LOG_WARNING("Failed to create database directory: " + ec.message());
    }

    // Create log directory
    std::filesystem::path const log_dir = log_file_path().parent_path();
    std::filesystem::create_directories(log_dir, ec);
    if (ec) {
        LOG_WARNING("Failed to create log directory: " + ec.message());
    }

    LOG_INFO("XDG directories ensured");
}

}  // namespace ip_server