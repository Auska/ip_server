#include "config.h"
#include "logger.h"
#include <iostream>
#include <sstream>

namespace ip_server {

ServerConfig ConfigParser::default_config() {
    ServerConfig config;
    return config;
}

ServerConfig ConfigParser::parse(int argc, char* argv[]) {
    ServerConfig config = default_config();

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--city-db" && i + 1 < argc) {
            config.city_db_path = argv[++i];
        } else if (arg == "--asn-db" && i + 1 < argc) {
            config.asn_db_path = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
            } catch (const std::exception& e) {
                LOG_ERROR("Invalid port number: " + std::string(argv[i]));
                throw std::runtime_error("Invalid port number");
            }
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            try {
                config.thread_pool_size = std::stoi(argv[++i]);
            } catch (const std::exception& e) {
                LOG_ERROR("Invalid thread count: " + std::string(argv[i]));
                throw std::runtime_error("Invalid thread count");
            }
        } else if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            std::exit(0);
        } else {
            std::ostringstream oss;
            oss << "Unknown option: " << arg;
            LOG_WARNING(oss.str());
        }
    }

    LOG_INFO("Configuration loaded:");
    LOG_INFO("  Host: " + config.host);
    LOG_INFO("  Port: " + std::to_string(config.port));
    LOG_INFO("  City DB: " + config.city_db_path);
    LOG_INFO("  ASN DB: " + config.asn_db_path);
    LOG_INFO("  Threads: " + std::to_string(config.thread_pool_size));

    return config;
}

void ConfigParser::print_help(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n"
              << "IP Geolocation & AS Lookup Service\n\n"
              << "Options:\n"
              << "  --city-db <path>    Path to City MaxMind database\n"
              << "                      (default: db/GeoLite2-City.mmdb)\n"
              << "  --asn-db <path>     Path to ASN MaxMind database\n"
              << "                      (default: db/GeoLite2-ASN.mmdb)\n"
              << "  --host <address>    Server host address\n"
              << "                      (default: 0.0.0.0)\n"
              << "  --port <port>       Server port\n"
              << "                      (default: 8080)\n"
              << "  --threads <count>   Thread pool size\n"
              << "                      (default: 4)\n"
              << "  --help, -h          Show this help message\n\n"
              << "Examples:\n"
              << "  " << program_name << "\n"
              << "  " << program_name << " --port 9000\n"
              << "  " << program_name << " --city-db /path/to/GeoLite2-City.mmdb --asn-db /path/to/GeoLite2-ASN.mmdb\n"
              << "  " << program_name << " --host 127.0.0.1 --port 8080\n\n"
              << "API Endpoints:\n"
              << "  GET  /                       - Service information\n"
              << "  GET  /health                 - Health check\n"
              << "  GET  /lookup?ip=<address>     - Single IP lookup\n"
              << "  POST /lookup                 - Batch lookup\n"
              << "                                Body: {\"ips\": [\"1.1.1.1\", \"8.8.8.8\"]}\n\n"
              << "For more information, visit: https://dev.maxmind.com/geoip/geolite2-free-geolocation-data\n";
}

} // namespace ip_server