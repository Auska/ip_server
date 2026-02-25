#include "password_generator.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

#include "logger.h"

namespace ip_server {

PasswordGenerator::PasswordGenerator() {
    LOG_INFO("PasswordGenerator initialized");
}

bool PasswordGenerator::validate_config(const PasswordConfig& config, std::string& error_message) {
    if (config.length < 8) {
        error_message = "Password length must be at least 8 characters";
        return false;
    }

    if (config.length > 128) {
        error_message = "Password length must not exceed 128 characters";
        return false;
    }

    if (!config.uppercase && !config.lowercase && !config.digits && !config.symbols) {
        error_message = "At least one character type must be enabled";
        return false;
    }

    return true;
}

PasswordResult PasswordGenerator::generate(const PasswordConfig& config) {
    std::string error_message;
    if (!validate_config(config, error_message)) {
        throw std::runtime_error(error_message);
    }

    std::string pool = build_character_pool(config);
    if (pool.empty()) {
        throw std::runtime_error("Character pool is empty");
    }

    // Use thread_local random generator for better performance
    // Initialize with random_device seed
    thread_local std::mt19937_64 gen([]() -> uint64_t {
        std::random_device rd;
        return rd();
    }());

    std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);

    std::string password;
    password.reserve(config.length);

    // Generate password
    for (int i = 0; i < config.length; ++i) {
        password += pool[dist(gen)];
    }

    // Ensure at least one character from each enabled type
    std::vector<std::string> required_chars;
    if (config.uppercase) {
        std::string upper_set = UPPERCASE;
        if (config.exclude_similar) {
            for (char c : std::string_view(CONFUSING_UPPER)) {
                upper_set.erase(std::remove(upper_set.begin(), upper_set.end(), c),
                                upper_set.end());
            }
        }
        if (!upper_set.empty()) {
            required_chars.push_back(upper_set);
        }
    }
    if (config.lowercase) {
        std::string lower_set = LOWERCASE;
        if (config.exclude_similar) {
            for (char c : std::string_view(CONFUSING_LOWER)) {
                lower_set.erase(std::remove(lower_set.begin(), lower_set.end(), c),
                                lower_set.end());
            }
        }
        if (!lower_set.empty()) {
            required_chars.push_back(lower_set);
        }
    }
    if (config.digits) {
        std::string digit_set = DIGITS;
        if (config.exclude_similar) {
            for (char c : std::string_view(CONFUSING_DIGITS)) {
                digit_set.erase(std::remove(digit_set.begin(), digit_set.end(), c),
                                digit_set.end());
            }
        }
        if (!digit_set.empty()) {
            required_chars.push_back(digit_set);
        }
    }
    if (config.symbols && !std::string(SYMBOLS).empty()) {
        required_chars.push_back(SYMBOLS);
    }

    // Replace random positions with required characters
    std::uniform_int_distribution<size_t> pos_dist(0, config.length - 1);
    for (const auto& char_set : required_chars) {
        if (char_set.empty()) continue;
        std::uniform_int_distribution<size_t> char_dist(0, char_set.size() - 1);
        size_t pos    = pos_dist(gen);
        password[pos] = char_set[char_dist(gen)];
    }

    // Shuffle password
    std::shuffle(password.begin(), password.end(), gen);

    PasswordResult result;
    result.password = password;
    result.length   = static_cast<int>(password.length());
    result.entropy  = calculate_entropy(password, static_cast<int>(pool.size()));
    result.strength = get_strength_rating(result.entropy);

    return result;
}

std::vector<PasswordResult> PasswordGenerator::generate_batch(const PasswordConfig& config,
                                                              int count) {
    if (count <= 0) {
        throw std::runtime_error("Count must be positive");
    }

    if (count > 100) {
        throw std::runtime_error("Batch count cannot exceed 100");
    }

    std::vector<PasswordResult> results;
    results.reserve(count);

    for (int i = 0; i < count; ++i) {
        results.push_back(generate(config));
    }

    return results;
}

std::string PasswordGenerator::build_character_pool(const PasswordConfig& config) const {
    std::string pool;

    if (config.uppercase) {
        std::string upper_set = UPPERCASE;
        if (config.exclude_similar) {
            for (char c : std::string_view(CONFUSING_UPPER)) {
                upper_set.erase(std::remove(upper_set.begin(), upper_set.end(), c),
                                upper_set.end());
            }
        }
        pool += upper_set;
    }

    if (config.lowercase) {
        std::string lower_set = LOWERCASE;
        if (config.exclude_similar) {
            for (char c : std::string_view(CONFUSING_LOWER)) {
                lower_set.erase(std::remove(lower_set.begin(), lower_set.end(), c),
                                lower_set.end());
            }
        }
        pool += lower_set;
    }

    if (config.digits) {
        std::string digit_set = DIGITS;
        if (config.exclude_similar) {
            for (char c : std::string_view(CONFUSING_DIGITS)) {
                digit_set.erase(std::remove(digit_set.begin(), digit_set.end(), c),
                                digit_set.end());
            }
        }
        pool += digit_set;
    }

    if (config.symbols) {
        pool += SYMBOLS;
    }

    return pool;
}

double PasswordGenerator::calculate_entropy(const std::string& password, int pool_size) {
    if (password.empty() || pool_size == 0) {
        return 0.0;
    }

    double log2_pool_size = std::log2(pool_size);
    return static_cast<double>(password.length()) * log2_pool_size;
}

std::string PasswordGenerator::get_strength_rating(double entropy) {
    if (entropy < 28.0) {
        return "very_weak";
    } else if (entropy < 36.0) {
        return "weak";
    } else if (entropy < 60.0) {
        return "fair";
    } else if (entropy < 80.0) {
        return "strong";
    } else {
        return "very_strong";
    }
}

}  // namespace ip_server