#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "maxmind_database.h"

namespace ip_server {

class ASNDatabase : public MaxMindDatabase {
   public:
    using MaxMindDatabase::MaxMindDatabase;

    nlohmann::json lookup(const std::string& ip_address) const;
};

}  // namespace ip_server
