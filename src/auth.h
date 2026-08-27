#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ip_server {

class APIAuth {
   public:
    explicit APIAuth(bool enabled = false);
    ~APIAuth() = default;

    void add_key(const std::string& key);

    bool is_valid(const std::string& key) const;

    bool load_keys_from_file(const std::string& filepath);

    size_t key_count() const;

   private:
    static std::string hash_key(const std::string& key);
    static std::string generate_key_id(const std::string& key_hash);

    bool enabled_;
    std::unordered_set<std::string> api_key_hashes_;
    mutable std::mutex mutex_;
};

}  // namespace ip_server
