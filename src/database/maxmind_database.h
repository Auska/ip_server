#pragma once

#include <maxminddb.h>

#include <nlohmann/json.hpp>
#include <string>

namespace ip_server {

/// Opens the .mmdb file in the constructor and throws std::runtime_error on
/// failure; the database stays open for the object's lifetime.
class MaxMindDatabase {
   public:
    explicit MaxMindDatabase(const std::string& db_path);
    virtual ~MaxMindDatabase();

    MaxMindDatabase(const MaxMindDatabase&) = delete;
    MaxMindDatabase& operator=(const MaxMindDatabase&) = delete;
    MaxMindDatabase(MaxMindDatabase&&) noexcept;
    MaxMindDatabase& operator=(MaxMindDatabase&&) noexcept;

   protected:
    MMDB_s mmdb_{};
    bool is_open_{false};  // false only in a moved-from database

    nlohmann::json perform_lookup(const std::string& ip_address, int& gai_error, int& mmdb_error,
                                  MMDB_lookup_result_s& result) const;

    /// Read an entry value by key path (e.g. "country", "iso_code"). Returns true
    /// and fills the entry data when the key exists; otherwise false.
    template <typename... Keys>
    static bool get_value(MMDB_entry_s& entry, MMDB_entry_data_s& data, Keys... keys) {
        const char* const path[] = {keys...};
        return MMDB_aget_value(&entry, &data, path) == MMDB_SUCCESS && data.has_data;
    }
};

}  // namespace ip_server
