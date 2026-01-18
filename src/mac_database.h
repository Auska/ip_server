#pragma once

#include <sqlite3.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace ip_server {

class OUIDatabase {
   public:
    OUIDatabase() = default;
    ~OUIDatabase();

    OUIDatabase(const OUIDatabase&)            = delete;
    OUIDatabase& operator=(const OUIDatabase&) = delete;
    OUIDatabase(OUIDatabase&&) noexcept;
    OUIDatabase& operator=(OUIDatabase&&) noexcept;

    bool open(const std::string& db_path);
    void close();
    bool is_open() const { return is_open_.load(std::memory_order_acquire); }

    nlohmann::json lookup(const std::string& mac_address) const;

   private:
    static std::string normalize_mac_address(const std::string& mac_address);
    static std::string extract_oui(const std::string& normalized_mac);

    sqlite3* db_ = nullptr;
    std::atomic<bool> is_open_{false};
    mutable std::mutex open_close_mutex_;
    mutable std::mutex query_mutex_;
};

}  // namespace ip_server