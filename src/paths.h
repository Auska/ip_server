#pragma once

#include <filesystem>

namespace ip_server {
namespace paths {

/// Base directory: $HOME/.config/ip_local (falls back to /tmp when HOME is unset)
std::filesystem::path base_dir();

/// Default config file path
std::filesystem::path config_file();

/// Default database directory
std::filesystem::path database_dir();

/// Default database file paths
std::filesystem::path city_db_path();
std::filesystem::path asn_db_path();
std::filesystem::path oui_db_path();

/// Default log file path
std::filesystem::path log_file_path();

/// Ensure base/database/log directories exist
void ensure_directories();

}  // namespace paths
}  // namespace ip_server
