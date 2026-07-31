#include "auth.h"

#include <openssl/sha.h>

#include <array>
#include <fstream>

#include "logger.h"

namespace ip_server {

namespace {

std::string sha256Hash(const std::string& input) {
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash;
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash.data());

    static constexpr char hex[] = "0123456789abcdef";
    std::string result(SHA256_DIGEST_LENGTH * 2, '\0');
    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        result[i * 2]     = hex[hash[i] >> 4];
        result[i * 2 + 1] = hex[hash[i] & 0xf];
    }
    return result;
}

}  // namespace

APIAuth::APIAuth(bool enabled) : enabled_(enabled) {
    if (enabled_) {
        LOG_INFO("API authentication enabled with secure key hashing");
    } else {
        LOG_INFO("API authentication disabled");
    }
}

std::string APIAuth::hash_key(const std::string& key) {
    return sha256Hash(key);
}

std::string APIAuth::generate_key_id(const std::string& key_hash) {
    if (key_hash.size() >= 8) {
        return "key_" + key_hash.substr(0, 8);
    }
    return "key_unknown";
}

void APIAuth::add_key(const std::string& key) {
    if (key.empty()) {
        LOG_WARNING("Attempted to add empty API key");
        return;
    }

    std::scoped_lock const lock(mutex_);
    std::string const key_hash = hash_key(key);
    std::string const key_id   = generate_key_id(key_hash);

    if (api_key_hashes_.insert(key_hash).second) {
        key_id_map_[key_hash] = key_id;
        LOG_INFO("Added API key: " + key_id);
    } else {
        LOG_WARNING("API key already exists: " + key_id);
    }
}

void APIAuth::remove_key(const std::string& key) {
    std::scoped_lock const lock(mutex_);
    std::string const key_hash = hash_key(key);
    std::string const key_id   = generate_key_id(key_hash);

    if (api_key_hashes_.erase(key_hash) > 0) {
        key_id_map_.erase(key_hash);
        LOG_INFO("Removed API key: " + key_id);
    }
}

bool APIAuth::is_valid(const std::string& key) const {
    if (!enabled_) {
        return true;
    }

    if (key.empty()) {
        return false;
    }

    std::string const key_hash = hash_key(key);
    std::scoped_lock const lock(mutex_);
    return api_key_hashes_.contains(key_hash);
}

bool APIAuth::load_keys_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open API keys file: " + filepath);
        return false;
    }

    std::scoped_lock const lock(mutex_);
    api_key_hashes_.clear();
    key_id_map_.clear();

    std::string line;
    size_t count = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t const start = line.find_first_not_of(" \t\r\n");
        size_t const end   = line.find_last_not_of(" \t\r\n");

        if (start != std::string::npos && end != std::string::npos) {
            std::string const key      = line.substr(start, end - start + 1);
            std::string const key_hash = hash_key(key);
            std::string const key_id   = generate_key_id(key_hash);

            if (api_key_hashes_.insert(key_hash).second) {
                key_id_map_[key_hash] = key_id;
                count++;
            }
        }
    }

    file.close();
    LOG_INFO("Loaded " + std::to_string(count) + " API keys from: " + filepath);
    return true;
}

size_t APIAuth::key_count() const {
    std::scoped_lock const lock(mutex_);
    return api_key_hashes_.size();
}

}  // namespace ip_server
