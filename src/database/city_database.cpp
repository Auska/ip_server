#include "city_database.h"

namespace ip_server {

nlohmann::json CityDatabase::lookup(const std::string& ip_address) const {
    int gai_error  = 0;
    int mmdb_error = 0;
    MMDB_lookup_result_s lookup_result;
    nlohmann::json result = perform_lookup(ip_address, gai_error, mmdb_error, lookup_result);

    if (result.contains("error") || !result.value("found", false)) {
        return result;
    }

    MMDB_entry_data_s entry_data;

    if (get_value(lookup_result.entry, entry_data, "country", "names", "en")) {
        result["country"] = std::string(entry_data.utf8_string, entry_data.data_size);
    }
    if (get_value(lookup_result.entry, entry_data, "country", "iso_code")) {
        result["country_code"] = std::string(entry_data.utf8_string, entry_data.data_size);
    }
    if (get_value(lookup_result.entry, entry_data, "city", "names", "en")) {
        result["city"] = std::string(entry_data.utf8_string, entry_data.data_size);
    }
    if (get_value(lookup_result.entry, entry_data, "continent", "names", "en")) {
        result["continent"] = std::string(entry_data.utf8_string, entry_data.data_size);
    }
    if (get_value(lookup_result.entry, entry_data, "location", "latitude")) {
        result["latitude"] = entry_data.double_value;
    }
    if (get_value(lookup_result.entry, entry_data, "location", "longitude")) {
        result["longitude"] = entry_data.double_value;
    }
    if (get_value(lookup_result.entry, entry_data, "location", "time_zone")) {
        result["timezone"] = std::string(entry_data.utf8_string, entry_data.data_size);
    }

    return result;
}

}  // namespace ip_server
