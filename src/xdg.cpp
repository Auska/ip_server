#include "xdg.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>

#include "logger.h"

namespace ip_server::xdg {

namespace {

constexpr const char* APP_NAME = "ip-server";

std::string get_env(const char* name, const std::string& default_value) {
    const char* value = std::getenv(name);
    return (value != nullptr) ? value : default_value;
}

std::filesystem::path get_home() {
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

}  // namespace

std::filesystem::path config_home() {
    std::string path = get_env("XDG_CONFIG_HOME", "");
    if (!path.empty()) {
        return path;
    }
    return get_home() / ".config";
}

std::filesystem::path data_home() {
    std::string path = get_env("XDG_DATA_HOME", "");
    if (!path.empty()) {
        return path;
    }
    return get_home() / ".local" / "share";
}

std::filesystem::path cache_home() {
    std::string path = get_env("XDG_CACHE_HOME", "");
    if (!path.empty()) {
        return path;
    }
    return get_home() / ".cache";
}

std::filesystem::path app_config_dir() {
    return config_home() / APP_NAME;
}

std::filesystem::path app_data_dir() {
    return data_home() / APP_NAME;
}

std::filesystem::path app_cache_dir() {
    return cache_home() / APP_NAME;
}

std::filesystem::path config_file() {
    return app_config_dir() / "config.json";
}

std::filesystem::path database_dir() {
    return app_data_dir() / "databases";
}

std::filesystem::path city_db_path() {
    return database_dir() / "GeoLite2-City.mmdb";
}

std::filesystem::path asn_db_path() {
    return database_dir() / "GeoLite2-ASN.mmdb";
}

std::filesystem::path oui_db_path() {
    return database_dir() / "master_oui.db";
}

std::filesystem::path log_file_path() {
    std::string const state_path = get_env("XDG_STATE_HOME", "");
    if (!state_path.empty()) {
        return std::filesystem::path(state_path) / APP_NAME / "logs" / "ip_server.log";
    }
    // Fallback to data home when XDG_STATE_HOME is unset
    return data_home() / APP_NAME / "logs" / "ip_server.log";
}

void ensure_directories() {
    const std::filesystem::path dirs[] = {app_config_dir(), app_data_dir(), app_cache_dir(),
                                          database_dir(), log_file_path().parent_path()};
    std::error_code ec;
    for (const auto& dir : dirs) {
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            LOG_WARNING("Failed to create directory: " + dir.string() + ": " + ec.message());
            ec.clear();
        }
    }

    LOG_INFO("XDG directories ensured");
}

}  // namespace ip_server::xdg