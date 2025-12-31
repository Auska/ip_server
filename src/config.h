#pragma once

#include <string>
#include <cstdint>

namespace ip_server {

struct ServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    std::string city_db_path = "db/GeoLite2-City.mmdb";
    std::string asn_db_path = "db/GeoLite2-ASN.mmdb";
    int thread_pool_size = 4;
};

class ConfigParser {
public:
    static ServerConfig parse(int argc, char* argv[]);
    static void print_help(const char* program_name);
    static ServerConfig default_config();
};

} // namespace ip_server