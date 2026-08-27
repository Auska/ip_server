#include "maxmind_database.h"

#include "logger.h"

namespace ip_server {

MaxMindDatabase::~MaxMindDatabase() {
    close();
}

MaxMindDatabase::MaxMindDatabase(MaxMindDatabase&& other) noexcept
    : mmdb_(other.mmdb_), is_open_(other.is_open_.load(std::memory_order_acquire)) {
    other.is_open_.store(false, std::memory_order_release);
    other.mmdb_ = {};
}

MaxMindDatabase& MaxMindDatabase::operator=(MaxMindDatabase&& other) noexcept {
    if (this != &other) {
        close();
        mmdb_ = other.mmdb_;
        is_open_.store(other.is_open_.load(std::memory_order_acquire), std::memory_order_release);
        other.is_open_.store(false, std::memory_order_release);
        other.mmdb_ = {};
    }
    return *this;
}

bool MaxMindDatabase::open(const std::string& db_path) {
    std::scoped_lock const lock(open_close_mutex_);

    if (is_open_.load(std::memory_order_acquire)) {
        close();
    }

    int const status = MMDB_open(db_path.c_str(), MMDB_MODE_MMAP, &mmdb_);
    if (status != MMDB_SUCCESS) {
        LOG_ERROR("Failed to open MaxMind DB '" + db_path + "': " + MMDB_strerror(status));
        return false;
    }

    is_open_.store(true, std::memory_order_release);
    LOG_INFO("Opened MaxMind database: " + db_path);
    return true;
}

void MaxMindDatabase::close() {
    std::scoped_lock const lock(open_close_mutex_);

    if (is_open_.load(std::memory_order_acquire)) {
        MMDB_close(&mmdb_);
        is_open_.store(false, std::memory_order_release);
        LOG_INFO("Closed MaxMind database");
    }
}

nlohmann::json MaxMindDatabase::perform_lookup(const std::string& ip_address, int& gai_error,
                                               int& mmdb_error,
                                               MMDB_lookup_result_s& result) const {
    nlohmann::json json_result;

    if (!is_open_) {
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
