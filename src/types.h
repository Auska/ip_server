#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace ip_server {

struct IPGeoInfo {
    std::string ip;
    bool found = false;
    std::string error;

    // Location info
    std::string country;
    std::string country_code;
    std::string city;
    std::string continent;
    double latitude = 0.0;
    double longitude = 0.0;
    std::string timezone;

    // AS info
    std::string as_organization;
    uint32_t as_number = 0;

    nlohmann::json to_json() const;
};

} // namespace ip_server