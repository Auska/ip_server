#include "paths.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>

#include "logger.h"

namespace ip_server::paths {

namespace {

constexpr const char* APP_DIR = "ip_local";

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

std::filesystem::path base_dir() {
    return get_home() / ".config" / APP_DIR;
}

std::filesystem::path config_file() {
    return base_dir() / "config.json";
}

std::filesystem::path database_dir() {
    return base_dir() / "databases";
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
    return base_dir() / "logs" / "ip_server.log";
}

void ensure_directories() {
    const std::filesystem::path dirs[] = {base_dir(), database_dir(),
                                          log_file_path().parent_path()};
    std::error_code ec;
    for (const auto& dir : dirs) {
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            LOG_WARNING("Failed to create directory: " + dir.string() + ": " + ec.message());
            ec.clear();
        }
    }

    LOG_INFO("Directories ensured under " + base_dir().string());
}

}  // namespace ip_server::paths
