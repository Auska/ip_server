#include "types.h"

namespace ip_server {

nlohmann::json IPGeoInfo::to_json() const {
    nlohmann::json j;
    j["ip"] = ip;
    j["found"] = found;

    if (!error.empty()) {
        j["error"] = error;
        return j;
    }

    if (found) {
        if (!country.empty()) j["country"] = country;
        if (!country_code.empty()) j["country_code"] = country_code;
        if (!city.empty()) j["city"] = city;
        if (!continent.empty()) j["continent"] = continent;
        if (latitude != 0.0) j["latitude"] = latitude;
        if (longitude != 0.0) j["longitude"] = longitude;
        if (!timezone.empty()) j["timezone"] = timezone;
        if (!as_organization.empty()) j["as_organization"] = as_organization;
        if (as_number != 0) j["as_number"] = as_number;
    }

    return j;
}

} // namespace ip_server