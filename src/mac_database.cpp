#include "mac_database.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <regex>
#include <stdexcept>

#include "logger.h"

namespace ip_server {

namespace {

struct SQLiteStmtDeleter {
    void operator()(sqlite3_stmt* stmt) const {
        if (stmt != nullptr) {
            sqlite3_finalize(stmt);
        }
    }
};

using SQLiteStmtPtr = std::unique_ptr<sqlite3_stmt, SQLiteStmtDeleter>;

}  // namespace

OUIDatabase::~OUIDatabase() {
    close();
}

OUIDatabase::OUIDatabase(OUIDatabase&& other) noexcept
    : db_(other.db_), is_open_(other.is_open_.load(std::memory_order_acquire)) {
    other.is_open_.store(false, std::memory_order_release);
    other.db_ = nullptr;
}

OUIDatabase& OUIDatabase::operator=(OUIDatabase&& other) noexcept {
    if (this != &other) {
        close();
        db_ = other.db_;
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
    status = sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &error_msg);
    if (status != SQLITE_OK) {
        LOG_WARNING("Failed to enable WAL mode: " + std::string(error_msg ? error_msg : "unknown"));
        sqlite3_free(error_msg);
    }

    is_open_.store(true, std::memory_order_release);
    LOG_INFO("Opened OUI database: " + db_path);
    return true;
}

void OUIDatabase::close() {
    std::scoped_lock const lock(open_close_mutex_);

    if (is_open_.load(std::memory_order_acquire) && (db_ != nullptr)) {
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

    std::scoped_lock const lock(query_mutex_);

    const char* sql =
        "SELECT oui, manufacturer, registry, short_name, "
        "device_type, registered_date, address, sources "
        "FROM oui_registry WHERE oui = ?";

    sqlite3_stmt* raw_stmt = nullptr;
    int status = sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr);

    if (status != SQLITE_OK) {
        result["error"] = std::string("Failed to prepare statement: ") + sqlite3_errmsg(db_);
        return result;
    }

    SQLiteStmtPtr const stmt(raw_stmt);

    status = sqlite3_bind_text(stmt.get(), 1, oui.c_str(), -1, SQLITE_TRANSIENT);
    if (status != SQLITE_OK) {
        result["error"] = std::string("Failed to bind parameter: ") + sqlite3_errmsg(db_);
        return result;
    }

    status = sqlite3_step(stmt.get());

    if (status == SQLITE_ROW) {
        result["mac"] = mac_address;
        result["oui"] = oui;
        result["found"] = true;

        const char* col;

        col = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        if (col != nullptr) { result["manufacturer"] = col;
}

        col = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        if (col != nullptr) { result["registry"] = col;
}

        col = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        if (col != nullptr) { result["short_name"] = col;
}

        col = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));
        if (col != nullptr) { result["device_type"] = col;
}

        col = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 5));
        if (col != nullptr) { result["registered_date"] = col;
}

        col = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 6));
        if (col != nullptr) { result["address"] = col;
}

        col = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 7));
        if (col != nullptr) { result["sources"] = col;
}

    } else if (status == SQLITE_DONE) {
        result["mac"] = mac_address;
        result["oui"] = oui;
        result["found"] = false;
    } else {
        result["error"] = std::string("Query failed: ") + sqlite3_errmsg(db_);
    }

    return result;
}

}  // namespace ip_server
