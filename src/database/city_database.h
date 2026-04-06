#pragma once

#include "maxmind_database.h"

namespace ip_server {

class CityDatabase : public MaxMindDatabase {
   public:
    nlohmann::json lookup(const std::string& ip_address) const;
};

}  // namespace ip_server
