#include "mac_database.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>

#include "logger.h"

namespace ip_server {

namespace {

constexpr const char* OUI_LOOKUP_SQL =
    "SELECT oui, manufacturer, registry, short_name, "
    "device_type, registered_date, address, sources "
    "FROM oui_registry WHERE oui = ?";

}  // namespace

OUIDatabase::~OUIDatabase() {
    close();
}

OUIDatabase::OUIDatabase(OUIDatabase&& other) noexcept
    : db_(other.db_),
      lookup_stmt_(std::move(other.lookup_stmt_)),
      is_open_(other.is_open_.load(std::memory_order_acquire)) {
    other.is_open_.store(false, std::memory_order_release);
    other.db_ = nullptr;
}

OUIDatabase& OUIDatabase::operator=(OUIDatabase&& other) noexcept {
    if (this != &other) {
        close();
        db_          = other.db_;
        lookup_stmt_ = std::move(other.lookup_stmt_);
        is_open_.store(other.is_open_.load(std::memory_order_acquire), std::memory_order_release);
        other.is_open_.store(false, std::memory_order_release);
        other.db_ = nullptr;
    }
    return *this;
}

bool OUIDatabase::open(const std::string& db_path) {
    std::scoped_lock const lock(open_close_mutex_);

    if (is_open_.load(std::memory_order_acquire)) {
        close();
    }

    int status = sqlite3_open_v2(db_path.c_str(), &db_,
                                 SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);

    if (status != SQLITE_OK) {
        LOG_ERROR("Failed to open OUI database '" + db_path + "': " + sqlite3_errmsg(db_));
        if (db_ != nullptr) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    char* error_msg = nullptr;
    status          = sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &error_msg);
    if (status != SQLITE_OK) {
        LOG_WARNING("Failed to enable WAL mode: " + std::string(error_msg ? error_msg : "unknown"));
        sqlite3_free(error_msg);
    }

    sqlite3_stmt* raw_stmt = nullptr;
    status                 = sqlite3_prepare_v2(db_, OUI_LOOKUP_SQL, -1, &raw_stmt, nullptr);
    if (status != SQLITE_OK) {
        LOG_ERROR("Failed to prepare OUI lookup statement: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    lookup_stmt_.reset(raw_stmt);

    is_open_.store(true, std::memory_order_release);
    LOG_INFO("Opened OUI database: " + db_path);
    return true;
}

void OUIDatabase::close() {
    std::scoped_lock const lock(open_close_mutex_);

    if (is_open_.load(std::memory_order_acquire) && (db_ != nullptr)) {
        lookup_stmt_.reset();
        sqlite3_close(db_);
        db_ = nullptr;
        is_open_.store(false, std::memory_order_release);
        LOG_INFO("Closed OUI database");
    }
}

std::string OUIDatabase::normalize_mac_address(const std::string& mac_address) {
    std::string normalized;
    normalized.reserve(mac_address.size());

    for (char const c : mac_address) {
        if (std::isxdigit(c) != 0) {
            normalized += std::toupper(c);
        }
    }

    return normalized;
}

std::string OUIDatabase::extract_oui(const std::string& normalized_mac) {
    if (normalized_mac.size() >= 6) {
        std::string const oui = normalized_mac.substr(0, 6);
        return oui.substr(0, 2) + ":" + oui.substr(2, 2) + ":" + oui.substr(4, 2);
    }
    return "";
}

nlohmann::json OUIDatabase::lookup(const std::string& mac_address) const {
    nlohmann::json result;

    if (!is_open_) {
        result["error"] = "Database not open";
        return result;
    }

    std::string const normalized = normalize_mac_address(mac_address);

    if (normalized.size() != 12) {
        result["error"] = "Invalid MAC address format";
        return result;
    }

    std::string const oui = extract_oui(normalized);

    if (!lookup_stmt_) {
        result["error"] = "Lookup statement not prepared";
        return result;
    }

    std::scoped_lock const lock(query_mutex_);

    sqlite3_reset(lookup_stmt_.get());
    sqlite3_clear_bindings(lookup_stmt_.get());

    int status = sqlite3_bind_text(lookup_stmt_.get(), 1, oui.c_str(), -1, SQLITE_TRANSIENT);
    if (status != SQLITE_OK) {
        result["error"] = std::string("Failed to bind parameter: ") + sqlite3_errmsg(db_);
        return result;
    }

    status = sqlite3_step(lookup_stmt_.get());

    if (status == SQLITE_ROW) {
        result["mac"]   = mac_address;
        result["oui"]   = oui;
        result["found"] = true;

        auto extractCol = [&](int col_idx, const char* key) {
            const char* col =
                reinterpret_cast<const char*>(sqlite3_column_text(lookup_stmt_.get(), col_idx));
            if (col != nullptr) result[key] = col;
        };

        extractCol(1, "manufacturer");
        extractCol(2, "registry");
        extractCol(3, "short_name");
        extractCol(4, "device_type");
        extractCol(5, "registered_date");
        extractCol(6, "address");
        extractCol(7, "sources");

    } else if (status == SQLITE_DONE) {
        result["mac"]   = mac_address;
        result["oui"]   = oui;
        result["found"] = false;
    } else {
        result["error"] = std::string("Query failed: ") + sqlite3_errmsg(db_);
    }

    return result;
}

}  // namespace ip_server
