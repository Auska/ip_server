#pragma once

#include <maxminddb.h>
#include <nlohmann/json.hpp>
#include <string>
#include <memory>

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
    bool is_open() const { return is_open_; }

    nlohmann::json lookup(const std::string& ip_address) const;

protected:
    MMDB_s mmdb_{};
    bool is_open_ = false;
};

class CityDatabase : public MaxMindDatabase {
public:
    nlohmann::json lookup(const std::string& ip_address) const;
};

class ASNDatabase : public MaxMindDatabase {
public:
    nlohmann::json lookup(const std::string& ip_address) const;
};

class IPGeoService {
public:
    explicit IPGeoService(const std::string& city_db_path, const std::string& asn_db_path);
    ~IPGeoService() = default;

    IPGeoService(const IPGeoService&) = delete;
    IPGeoService& operator=(const IPGeoService&) = delete;

    nlohmann::json lookup(const std::string& ip_address) const;

private:
    CityDatabase city_db_;
    ASNDatabase asn_db_;
    mutable nlohmann::json cache_;
};

} // namespace ip_server