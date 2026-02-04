#include "auth.h"

#include <fstream>
#include <sstream>

#include "logger.h"

namespace ip_server {

APIAuth::APIAuth(bool enabled) : enabled_(enabled) {
    if (enabled_) {
        LOG_INFO("API authentication enabled");
    } else {
        LOG_INFO("API authentication disabled");
    }
}

void APIAuth::add_key(const std::string& key) {
    if (key.empty()) {
        LOG_WARNING("Attempted to add empty API key");
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (api_keys_.insert(key).second) {
        LOG_INFO("Added API key: " + key.substr(0, 8) + "...");
    } else {
        LOG_WARNING("API key already exists: " + key.substr(0, 8) + "...");
    }
}

void APIAuth::remove_key(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (api_keys_.erase(key) > 0) {
        LOG_INFO("Removed API key: " + key.substr(0, 8) + "...");
    }
}

bool APIAuth::is_valid(const std::string& key) const {
    if (!enabled_) {
        return true;  // If auth is disabled, all requests are allowed
    }

    std::lock_guard<std::mutex> lock(mutex_);
    return api_keys_.find(key) != api_keys_.end();
}

bool APIAuth::load_keys_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open API keys file: " + filepath);
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    api_keys_.clear();

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end   = line.find_last_not_of(" \t\r\n");

        if (start != std::string::npos && end != std::string::npos) {
            std::string key = line.substr(start, end - start + 1);
            api_keys_.insert(key);
        }
    }

    file.close();
    LOG_INFO("Loaded " + std::to_string(api_keys_.size()) + " API keys from: " + filepath);
    return true;
}

size_t APIAuth::key_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return api_keys_.size();
}

}  // namespace ip_server