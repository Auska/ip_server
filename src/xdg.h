#pragma once

#include <filesystem>
#include <string>

namespace ip_server {

class XDGPaths {
   public:
    // Get XDG base directories
    static std::filesystem::path config_home() ;
    static std::filesystem::path data_home() ;
    static std::filesystem::path cache_home() ;

    // Get application-specific paths
    static std::filesystem::path app_config_dir();
    static std::filesystem::path app_data_dir();
    static std::filesystem::path app_cache_dir();

    // Get default config file path
    static std::filesystem::path config_file();

    // Get default database directory
    static std::filesystem::path database_dir();

    // Get default database file paths
    static std::filesystem::path city_db_path() ;
    static std::filesystem::path asn_db_path() ;
    static std::filesystem::path oui_db_path() ;

    // Get default log file path
    static std::filesystem::path log_file_path();

    // Ensure directories exist
    static void ensure_directories() ;

   private:
    static std::string get_env(const char* name, const std::string& default_value);
    static std::filesystem::path get_home();

    static constexpr const char* APP_NAME = "ip-server";
};

}  // namespace ip_server