#include "password_generator.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

#include "logger.h"

namespace ip_server {

namespace {

thread_local std::mt19937_64 password_gen = [] {
    std::random_device rd;
    std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
    return std::mt19937_64(seq);
}();

}  // namespace

PasswordGenerator::PasswordGenerator() {
    LOG_INFO("PasswordGenerator initialized with secure seed");
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

    auto sets = build_character_sets(config);
    if (sets.pool.empty()) {
        throw std::runtime_error("Character pool is empty");
    }

    std::uniform_int_distribution<size_t> dist(0, sets.pool.size() - 1);

    std::string password;
    password.reserve(config.length);

    for (int i = 0; i < config.length; ++i) {
        password += sets.pool[dist(password_gen)];
    }

    std::uniform_int_distribution<size_t> pos_dist(0, config.length - 1);
    for (const auto& char_set : sets.required_chars) {
        if (char_set.empty()) { continue; }
        std::uniform_int_distribution<size_t> char_dist(0, char_set.size() - 1);
        size_t const pos = pos_dist(password_gen);
        password[pos] = char_set[char_dist(password_gen)];
    }

    std::shuffle(password.begin(), password.end(), password_gen);

    PasswordResult result;
    result.password = password;
    result.length = static_cast<int>(password.length());
    result.entropy = calculate_entropy(password, static_cast<int>(sets.pool.size()));
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

PasswordGenerator::CharacterSets PasswordGenerator::build_character_sets(const PasswordConfig& config) {
    CharacterSets sets;

    auto add_set = [&](const char* chars, const char* confusable, bool enabled) {
        if (!enabled) { return; }
        std::string filtered(chars);
        if (config.exclude_similar && confusable) {
            for (char const c : std::string_view(confusable)) {
                filtered.erase(std::remove(filtered.begin(), filtered.end(), c), filtered.end());
            }
        }
        sets.pool += filtered;
        if (!filtered.empty()) {
            sets.required_chars.push_back(std::move(filtered));
        }
    };

    add_set(UPPERCASE, CONFUSING_UPPER, config.uppercase);
    add_set(LOWERCASE, CONFUSING_LOWER, config.lowercase);
    add_set(DIGITS, CONFUSING_DIGITS, config.digits);
    add_set(SYMBOLS, nullptr, config.symbols);

    return sets;
}

std::string PasswordGenerator::build_character_pool(const PasswordConfig& config) {
    return build_character_sets(config).pool;
}

double PasswordGenerator::calculate_entropy(const std::string& password, int pool_size) {
    if (password.empty() || pool_size == 0) {
        return 0.0;
    }

    double const log2_pool_size = std::log2(pool_size);
    return static_cast<double>(password.length()) * log2_pool_size;
}

std::string PasswordGenerator::get_strength_rating(double entropy) {
    if (entropy < 28.0) {
        return "very_weak";
    }
    if (entropy < 36.0) {
        return "weak";
    }
    if (entropy < 60.0) {
        return "fair";
    }
    if (entropy < 80.0) {
        return "strong";
    }
    return "very_strong";
}

}  // namespace ip_server
