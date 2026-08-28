#include "mac_database.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <utility>

#include "logger.h"

namespace ip_server {

namespace {

constexpr const char* OUI_LOOKUP_SQL =
    "SELECT oui, manufacturer, registry, short_name, "
    "device_type, registered_date, address, sources "
    "FROM oui_registry WHERE oui = ?";

}  // namespace

OUIDatabase::OUIDatabase(const std::string& db_path) {
    int status = sqlite3_open_v2(db_path.c_str(), &db_,
                                 SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);

    if (status != SQLITE_OK) {
        std::string const error = (db_ != nullptr) ? sqlite3_errmsg(db_) : "unknown error";
        if (db_ != nullptr) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        throw std::runtime_error("Failed to open OUI database '" + db_path + "': " + error);
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
        std::string const error = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to prepare OUI lookup statement: " + error);
    }
    lookup_stmt_.reset(raw_stmt);

    is_open_ = true;
    LOG_INFO("Opened OUI database: " + db_path);
}

OUIDatabase::~OUIDatabase() {
    if (is_open_) {
        lookup_stmt_.reset();
        sqlite3_close(db_);
    }
}

OUIDatabase::OUIDatabase(OUIDatabase&& other) noexcept
    : db_(other.db_),
      lookup_stmt_(std::move(other.lookup_stmt_)),
      is_open_(other.is_open_) {
    other.is_open_ = false;
    other.db_      = nullptr;
}

OUIDatabase& OUIDatabase::operator=(OUIDatabase&& other) noexcept {
    if (this != &other) {
        if (is_open_) {
            lookup_stmt_.reset();
            sqlite3_close(db_);
        }
        db_            = other.db_;
        lookup_stmt_   = std::move(other.lookup_stmt_);
        is_open_       = other.is_open_;
        other.is_open_ = false;
        other.db_      = nullptr;
    }
    return *this;
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

    if (!is_open_) {  // moved-from database
        result["error"] = "Database not open";
        return result;
    }

    std::string const normalized = normalize_mac_address(mac_address);

    if (normalized.size() != 12) {
        result["error"] = "Invalid MAC address format";
        return result;
    }

    std::string const oui = extract_oui(normalized);

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

        static constexpr std::pair<int, const char*> OUI_COLUMNS[] = {
            {1, "manufacturer"}, {2, "registry"},       {3, "short_name"},
            {4, "device_type"},  {5, "registered_date"}, {6, "address"},
            {7, "sources"}};
        for (const auto& [col_idx, key] : OUI_COLUMNS) {
            const char* col =
                reinterpret_cast<const char*>(sqlite3_column_text(lookup_stmt_.get(), col_idx));
            if (col != nullptr) result[key] = col;
        }

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
