#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ip_server {

class APIAuth {
   public:
    explicit APIAuth(bool enabled = false);
    ~APIAuth() = default;

    void add_key(const std::string& key);

    void remove_key(const std::string& key);

    bool is_valid(const std::string& key) const;

    bool is_enabled() const { return enabled_; }

    void set_enabled(bool enabled) { enabled_ = enabled; }

    bool load_keys_from_file(const std::string& filepath);

    size_t key_count() const;

    void set_trusted_proxies(const std::vector<std::string>& proxies) {
        std::lock_guard<std::mutex> lock(mutex_);
        trusted_proxies_ = proxies;
    }

    bool is_trusted_proxy(const std::string& ip) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (trusted_proxies_.empty()) {
            return true;
        }
        for (const auto& proxy : trusted_proxies_) {
            if (proxy == ip) {
                return true;
            }
        }
        return false;
    }

   private:
    static std::string hash_key(const std::string& key);
    static std::string generate_key_id(const std::string& key_hash);

    bool enabled_;
    std::unordered_set<std::string> api_key_hashes_;
    std::vector<std::string> trusted_proxies_;
    mutable std::mutex mutex_;
};

}  // namespace ip_server
