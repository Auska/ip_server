#include "database.h"
#include "types.h"
#include "logger.h"
#include <stdexcept>
#include <cstring>

namespace ip_server {

// MaxMindDatabase implementation

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
    std::lock_guard<std::mutex> lock(open_close_mutex_);

    if (is_open_.load(std::memory_order_acquire)) {
        close();
    }

    int status = MMDB_open(db_path.c_str(), MMDB_MODE_MMAP, &mmdb_);
    if (status != MMDB_SUCCESS) {
        LOG_ERROR("Failed to open MaxMind DB '" + db_path + "': " + MMDB_strerror(status));
        return false;
    }

    is_open_.store(true, std::memory_order_release);
    LOG_INFO("Opened MaxMind database: " + db_path);
    return true;
}

void MaxMindDatabase::close() {
    std::lock_guard<std::mutex> lock(open_close_mutex_);

    if (is_open_.load(std::memory_order_acquire)) {
        MMDB_close(&mmdb_);
        is_open_.store(false, std::memory_order_release);
        LOG_INFO("Closed MaxMind database");
    }
}

nlohmann::json MaxMindDatabase::lookup(const std::string& ip_address) const {
    nlohmann::json result;

    if (!is_open_) {
        result["error"] = "Database not open";
        return result;
    }

    int gai_error, mmdb_error;
    MMDB_lookup_result_s lookup_result =
        MMDB_lookup_string(&mmdb_, ip_address.c_str(), &gai_error, &mmdb_error);

    if (gai_error != 0) {
        result["error"] = "Invalid IP address";
        return result;
    }

    if (mmdb_error != MMDB_SUCCESS) {
        result["error"] = std::string("MaxMind DB lookup error: ") + MMDB_strerror(mmdb_error);
        return result;
    }

    if (!lookup_result.found_entry) {
        result["found"] = false;
        return result;
    }

    result["found"] = true;
    return result;
}

// CityDatabase implementation

nlohmann::json CityDatabase::lookup(const std::string& ip_address) const {
    auto result = MaxMindDatabase::lookup(ip_address);

    if (!result.value("found", false) || result.contains("error")) {
        result["ip"] = ip_address;
        return result;
    }

    int gai_error, mmdb_error;
    MMDB_lookup_result_s lookup_result =
        MMDB_lookup_string(&mmdb_, ip_address.c_str(), &gai_error, &mmdb_error);

    if (!lookup_result.found_entry) {
        result["ip"] = ip_address;
        return result;
    }

    result["ip"] = ip_address;

    MMDB_entry_data_s entry_data;
    int status;

    // Country
    status = MMDB_get_value(&lookup_result.entry, &entry_data, "country", "names", "en", nullptr);
    if (status == MMDB_SUCCESS && entry_data.has_data) {
        result["country"] = std::string(entry_data.utf8_string, entry_data.data_size);
    }

    status = MMDB_get_value(&lookup_result.entry, &entry_data, "country", "iso_code", nullptr);
    if (status == MMDB_SUCCESS && entry_data.has_data) {
        result["country_code"] = std::string(entry_data.utf8_string, entry_data.data_size);
    }

    // City
    status = MMDB_get_value(&lookup_result.entry, &entry_data, "city", "names", "en", nullptr);
    if (status == MMDB_SUCCESS && entry_data.has_data) {
        result["city"] = std::string(entry_data.utf8_string, entry_data.data_size);
    }

    // Continent
    status = MMDB_get_value(&lookup_result.entry, &entry_data, "continent", "names", "en", nullptr);
    if (status == MMDB_SUCCESS && entry_data.has_data) {
        result["continent"] = std::string(entry_data.utf8_string, entry_data.data_size);
    }

    // Location
    status = MMDB_get_value(&lookup_result.entry, &entry_data, "location", "latitude", nullptr);
    if (status == MMDB_SUCCESS && entry_data.has_data) {
        result["latitude"] = entry_data.double_value;
    }

    status = MMDB_get_value(&lookup_result.entry, &entry_data, "location", "longitude", nullptr);
    if (status == MMDB_SUCCESS && entry_data.has_data) {
        result["longitude"] = entry_data.double_value;
    }

    status = MMDB_get_value(&lookup_result.entry, &entry_data, "location", "time_zone", nullptr);
    if (status == MMDB_SUCCESS && entry_data.has_data) {
        result["timezone"] = std::string(entry_data.utf8_string, entry_data.data_size);
    }

    return result;
}

// ASNDatabase implementation

nlohmann::json ASNDatabase::lookup(const std::string& ip_address) const {
    auto result = MaxMindDatabase::lookup(ip_address);

    if (!result.value("found", false) || result.contains("error")) {
        result["ip"] = ip_address;
        return result;
    }

    int gai_error, mmdb_error;
    MMDB_lookup_result_s lookup_result =
        MMDB_lookup_string(&mmdb_, ip_address.c_str(), &gai_error, &mmdb_error);

    if (!lookup_result.found_entry) {
        result["ip"] = ip_address;
        return result;
    }

    result["ip"] = ip_address;

    MMDB_entry_data_s entry_data;
    int status;

    // AS Organization
    status = MMDB_get_value(&lookup_result.entry, &entry_data, "autonomous_system_organization", nullptr);
    if (status == MMDB_SUCCESS && entry_data.has_data) {
        result["as_organization"] = std::string(entry_data.utf8_string, entry_data.data_size);
    }

    // AS Number
    status = MMDB_get_value(&lookup_result.entry, &entry_data, "autonomous_system_number", nullptr);
    if (status == MMDB_SUCCESS && entry_data.has_data) {
        result["as_number"] = entry_data.uint32;
    }

    return result;
}

// IPGeoService implementation

IPGeoService::IPGeoService(const std::string& city_db_path, const std::string& asn_db_path,
                           size_t cache_size)
    : cache_(cache_size) {

    if (!city_db_.open(city_db_path)) {
        throw std::runtime_error("Failed to open City database: " + city_db_path);
    }

    if (!asn_db_.open(asn_db_path)) {
        throw std::runtime_error("Failed to open ASN database: " + asn_db_path);
    }

    LOG_INFO("IPGeoService initialized with cache size: " + std::to_string(cache_size));
}

nlohmann::json IPGeoService::lookup(const std::string& ip_address) const {
    // Check cache first
    if (cache_enabled_) {
        auto cached = cache_.get(ip_address);
        if (cached.has_value()) {
            LOG_DEBUG("Cache hit for IP: " + ip_address);
            return cached.value();
        }
    }

    nlohmann::json result;
    result["ip"] = ip_address;
    result["found"] = false;

    try {
        auto city_result = city_db_.lookup(ip_address);
        auto asn_result = asn_db_.lookup(ip_address);

        if (city_result.contains("error")) {
            result["error"] = city_result["error"];
            return result;
        }

        bool city_found = city_result.value("found", false);
        bool asn_found = asn_result.value("found", false);

        if (!city_found && !asn_found) {
            return result;
        }

        result["found"] = true;

        // Merge city information
        if (city_found) {
            if (city_result.contains("country")) result["country"] = city_result["country"];
            if (city_result.contains("country_code")) result["country_code"] = city_result["country_code"];
            if (city_result.contains("city")) result["city"] = city_result["city"];
            if (city_result.contains("continent")) result["continent"] = city_result["continent"];
            if (city_result.contains("latitude")) result["latitude"] = city_result["latitude"];
            if (city_result.contains("longitude")) result["longitude"] = city_result["longitude"];
            if (city_result.contains("timezone")) result["timezone"] = city_result["timezone"];
        }

        // Merge ASN information
        if (asn_found) {
            if (asn_result.contains("as_organization")) result["as_organization"] = asn_result["as_organization"];
            if (asn_result.contains("as_number")) result["as_number"] = asn_result["as_number"];
        }

        // Cache the result
        if (cache_enabled_ && result["found"]) {
            cache_.put(ip_address, result);
            LOG_DEBUG("Cached result for IP: " + ip_address);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error during IP lookup: " + std::string(e.what()));
        result["error"] = e.what();
    }

    return result;
}

} // namespace ip_server