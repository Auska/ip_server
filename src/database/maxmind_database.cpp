#include "maxmind_database.h"

#include <stdexcept>
#include <utility>

#include "logger.h"

namespace ip_server {

MaxMindDatabase::MaxMindDatabase(const std::string& db_path) {
    int const status = MMDB_open(db_path.c_str(), MMDB_MODE_MMAP, &mmdb_);
    if (status != MMDB_SUCCESS) {
        throw std::runtime_error("Failed to open MaxMind DB '" + db_path
                                 + "': " + MMDB_strerror(status));
    }

    is_open_ = true;
    LOG_INFO("Opened MaxMind database: " + db_path);
}

MaxMindDatabase::~MaxMindDatabase() {
    if (is_open_) {
        MMDB_close(&mmdb_);
    }
}

MaxMindDatabase::MaxMindDatabase(MaxMindDatabase&& other) noexcept
    : mmdb_(other.mmdb_), is_open_(other.is_open_) {
    other.is_open_ = false;
    other.mmdb_    = {};
}

MaxMindDatabase& MaxMindDatabase::operator=(MaxMindDatabase&& other) noexcept {
    if (this != &other) {
        if (is_open_) {
            MMDB_close(&mmdb_);
        }
        mmdb_          = other.mmdb_;
        is_open_       = other.is_open_;
        other.is_open_ = false;
        other.mmdb_    = {};
    }
    return *this;
}

nlohmann::json MaxMindDatabase::perform_lookup(const std::string& ip_address, int& gai_error,
                                               int& mmdb_error,
                                               MMDB_lookup_result_s& result) const {
    nlohmann::json json_result;

    if (!is_open_) {  // moved-from database
        json_result["error"] = "Database not open";
        json_result["ip"]    = ip_address;
        return json_result;
    }

    result = MMDB_lookup_string(&mmdb_, ip_address.c_str(), &gai_error, &mmdb_error);

    if (gai_error != 0) {
        json_result["error"] = "Invalid IP address";
        json_result["ip"]    = ip_address;
        return json_result;
    }

    if (mmdb_error != MMDB_SUCCESS) {
        json_result["error"] = std::string("MaxMind DB lookup error: ") + MMDB_strerror(mmdb_error);
        json_result["ip"]    = ip_address;
        return json_result;
    }

    if (!result.found_entry) {
        json_result["ip"]    = ip_address;
        json_result["found"] = false;
        return json_result;
    }

    json_result["ip"]    = ip_address;
    json_result["found"] = true;
    return json_result;
}

}  // namespace ip_server
