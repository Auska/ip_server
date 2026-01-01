#pragma once

#include <string>
#include <unordered_set>
#include <mutex>
#include <vector>

namespace ip_server {

class APIAuth {
public:
    explicit APIAuth(bool enabled = false);
    ~APIAuth() = default;

    // Add API key to whitelist
    void add_key(const std::string& key);

    // Remove API key from whitelist
    void remove_key(const std::string& key);

    // Check if API key is valid
    bool is_valid(const std::string& key) const;

    // Check if authentication is enabled
    bool is_enabled() const { return enabled_; }

    // Enable/disable authentication
    void set_enabled(bool enabled) { enabled_ = enabled; }

    // Load keys from file (one key per line)
    bool load_keys_from_file(const std::string& filepath);

    // Get number of registered keys
    size_t key_count() const;

private:
    bool enabled_;
    std::unordered_set<std::string> api_keys_;
    mutable std::mutex mutex_;
};

} // namespace ip_server