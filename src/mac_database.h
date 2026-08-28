#pragma once

#include <sqlite3.h>

#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace ip_server {

struct SQLiteStmtDeleter {
    void operator()(sqlite3_stmt* stmt) const noexcept {
        if (stmt) sqlite3_finalize(stmt);
    }
};

using SQLiteStmtPtr = std::unique_ptr<sqlite3_stmt, SQLiteStmtDeleter>;

/// Opens the OUI database in the constructor and throws std::runtime_error on
/// failure; the database stays open for the object's lifetime.
class OUIDatabase {
   public:
    explicit OUIDatabase(const std::string& db_path);
    ~OUIDatabase();

    OUIDatabase(const OUIDatabase&) = delete;
    OUIDatabase& operator=(const OUIDatabase&) = delete;
    OUIDatabase(OUIDatabase&&) noexcept;
    OUIDatabase& operator=(OUIDatabase&&) noexcept;

    nlohmann::json lookup(const std::string& mac_address) const;

   private:
    static std::string normalize_mac_address(const std::string& mac_address);
    static std::string extract_oui(const std::string& normalized_mac);

    sqlite3* db_ = nullptr;
    mutable SQLiteStmtPtr lookup_stmt_;
    bool is_open_ = false;  // false only in a moved-from database
    mutable std::mutex query_mutex_;  // serializes the shared prepared statement
};

}  // namespace ip_server
