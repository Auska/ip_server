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

void APIAuth::add_key(const std::string& key) {
    if (key.empty()) {
        LOG_WARNING("Attempted to add empty API key");
        return;
    }

    std::scoped_lock const lock(mutex_);
    std::string const key_hash = sha256Hash(key);

    if (api_key_hashes_.insert(key_hash).second) {
        LOG_INFO("Added API key: key_" + key_hash.substr(0, 8));
    } else {
        LOG_WARNING("API key already exists: key_" + key_hash.substr(0, 8));
    }
}

bool APIAuth::is_valid(const std::string& key) const {
    if (key.empty()) {
        return false;
    }

    std::scoped_lock const lock(mutex_);
    return api_key_hashes_.contains(sha256Hash(key));
}

bool APIAuth::load_keys_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open API keys file: " + filepath);
        return false;
    }

    std::scoped_lock const lock(mutex_);
    api_key_hashes_.clear();

    std::string line;
    size_t count = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t const start = line.find_first_not_of(" \t\r\n");
        size_t const end   = line.find_last_not_of(" \t\r\n");

        if (start != std::string::npos && end != std::string::npos) {
            std::string const key_hash = sha256Hash(line.substr(start, end - start + 1));

            if (api_key_hashes_.insert(key_hash).second) {
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
