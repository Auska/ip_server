#pragma once

#include <string>
#include <filesystem>

namespace ip_server {

class XDGPaths {
public:
    static XDGPaths& instance();

    // Get XDG base directories
    std::filesystem::path config_home() const;
    std::filesystem::path data_home() const;
    std::filesystem::path cache_home() const;
    std::filesystem::path state_home() const;

    // Get application-specific paths
    std::filesystem::path app_config_dir() const;
    std::filesystem::path app_data_dir() const;
    std::filesystem::path app_cache_dir() const;
    std::filesystem::path app_state_dir() const;

    // Get default config file path
    std::filesystem::path config_file() const;

    // Get default database directory
    std::filesystem::path database_dir() const;

    // Get default database file paths
    std::filesystem::path city_db_path() const;
    std::filesystem::path asn_db_path() const;
    std::filesystem::path oui_db_path() const;

    // Ensure directories exist
    void ensure_directories() const;

private:
    XDGPaths();

    std::string get_env(const char* name, const std::string& default_value) const;
    std::filesystem::path get_home() const;

    static constexpr const char* APP_NAME = "ip-server";
};

} // namespace ip_server