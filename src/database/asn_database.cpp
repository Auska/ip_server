#include "asn_database.h"

namespace ip_server {

nlohmann::json ASNDatabase::lookup(const std::string& ip_address) const {
    int gai_error  = 0;
    int mmdb_error = 0;
    MMDB_lookup_result_s lookup_result;
    nlohmann::json result = perform_lookup(ip_address, gai_error, mmdb_error, lookup_result);

    if (result.contains("error") || !result.value("found", false)) {
        return result;
    }

    MMDB_entry_data_s entry_data;
    int status = 0;

    status = MMDB_get_value(&lookup_result.entry, &entry_data, "autonomous_system_organization",
                            nullptr);
    if (status == MMDB_SUCCESS && entry_data.has_data) {
        result["as_organization"] = std::string(entry_data.utf8_string, entry_data.data_size);
    }

    status = MMDB_get_value(&lookup_result.entry, &entry_data, "autonomous_system_number", nullptr);
    if (status == MMDB_SUCCESS && entry_data.has_data) {
        result["as_number"] = entry_data.uint32;
    }

    return result;
}

}  // namespace ip_server
