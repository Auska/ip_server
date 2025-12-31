#include "xdg.h"
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include "logger.h"

namespace ip_server {

XDGPaths& XDGPaths::instance() {
    static XDGPaths instance;
    return instance;
}

XDGPaths::XDGPaths() {
    LOG_INFO("XDG paths initialized");
}

std::string XDGPaths::get_env(const char* name, const std::string& default_value) const {
    const char* value = std::getenv(name);
    return value ? value : default_value;
}

std::filesystem::path XDGPaths::get_home() const {
    const char* home = std::getenv("HOME");
    if (home) {
        return home;
    }

    // Fallback to getpwuid
    struct passwd* pw = getpwuid(getuid());
    if (pw) {
        return pw->pw_dir;
    }

    // Last resort
    return "/tmp";
}

std::filesystem::path XDGPaths::config_home() const {
    std::string path = get_env("XDG_CONFIG_HOME", "");
    if (!path.empty()) {
        return path;
    }
    return get_home() / ".config";
}

std::filesystem::path XDGPaths::data_home() const {
    std::string path = get_env("XDG_DATA_HOME", "");
    if (!path.empty()) {
        return path;
    }
    return get_home() / ".local" / "share";
}

std::filesystem::path XDGPaths::cache_home() const {
    std::string path = get_env("XDG_CACHE_HOME", "");
    if (!path.empty()) {
        return path;
    }
    return get_home() / ".cache";
}

std::filesystem::path XDGPaths::state_home() const {
    std::string path = get_env("XDG_STATE_HOME", "");
    if (!path.empty()) {
        return path;
    }
    return get_home() / ".local" / "state";
}

std::filesystem::path XDGPaths::app_config_dir() const {
    return config_home() / APP_NAME;
}

std::filesystem::path XDGPaths::app_data_dir() const {
    return data_home() / APP_NAME;
}

std::filesystem::path XDGPaths::app_cache_dir() const {
    return cache_home() / APP_NAME;
}

std::filesystem::path XDGPaths::app_state_dir() const {
    return state_home() / APP_NAME;
}

std::filesystem::path XDGPaths::config_file() const {
    return app_config_dir() / "config.toml";
}

std::filesystem::path XDGPaths::database_dir() const {
    return app_data_dir() / "databases";
}

std::filesystem::path XDGPaths::city_db_path() const {
    return database_dir() / "GeoLite2-City.mmdb";
}

std::filesystem::path XDGPaths::asn_db_path() const {
    return database_dir() / "GeoLite2-ASN.mmdb";
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

    LOG_INFO("XDG directories ensured");
}

} // namespace ip_server