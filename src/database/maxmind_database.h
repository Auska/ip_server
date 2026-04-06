#pragma once

#include <maxminddb.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace ip_server {

class MaxMindDatabase {
   public:
    MaxMindDatabase() = default;
    virtual ~MaxMindDatabase();

    MaxMindDatabase(const MaxMindDatabase&) = delete;
    MaxMindDatabase& operator=(const MaxMindDatabase&) = delete;
    MaxMindDatabase(MaxMindDatabase&&) noexcept;
    MaxMindDatabase& operator=(MaxMindDatabase&&) noexcept;

    bool open(const std::string& db_path);
    void close();
    bool is_open() const { return is_open_.load(std::memory_order_acquire); }

    nlohmann::json lookup(const std::string& ip_address) const;

   protected:
    MMDB_s mmdb_{};
    std::atomic<bool> is_open_{false};
    mutable std::mutex open_close_mutex_;

    nlohmann::json perform_lookup(const std::string& ip_address, int& gai_error, int& mmdb_error,
                                  MMDB_lookup_result_s& result) const;
};

}  // namespace ip_server
