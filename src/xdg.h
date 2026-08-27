#pragma once

#include <filesystem>
#include <string>

namespace ip_server {

namespace xdg {

// Get XDG base directories
std::filesystem::path config_home();
std::filesystem::path data_home();
std::filesystem::path cache_home();

// Get application-specific paths
std::filesystem::path app_config_dir();
std::filesystem::path app_data_dir();
std::filesystem::path app_cache_dir();

// Get default config file path
std::filesystem::path config_file();

// Get default database directory
std::filesystem::path database_dir();

// Get default database file paths
std::filesystem::path city_db_path();
std::filesystem::path asn_db_path();
std::filesystem::path oui_db_path();

// Get default log file path
std::filesystem::path log_file_path();

// Ensure directories exist
void ensure_directories();

}  // namespace xdg

}  // namespace ip_server