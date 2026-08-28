#pragma once

#include <string>

#include "cache.h"
#include "types.h"
#include "mac_database.h"

namespace ip_server {

class MACLookupService {
   public:
    explicit MACLookupService(const std::string& oui_db_path, size_t cache_size = 10000);
    ~MACLookupService() = default;

    MACLookupService(const MACLookupService&) = delete;
    MACLookupService& operator=(const MACLookupService&) = delete;

    LookupResult lookup(const std::string& mac_address) const;

    bool is_oui_db_open() const { return oui_db_.is_open(); }

    CacheStats cache_stats() const { return cache_.get_stats(); }

   private:
    OUIDatabase oui_db_;
    mutable IPCache cache_;
};

}  // namespace ip_server
