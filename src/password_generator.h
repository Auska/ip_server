#pragma once

#include <string>
#include <vector>

namespace ip_server {

struct PasswordConfig {
    int length_           = 16;
    bool uppercase_       = true;
    bool lowercase_       = true;
    bool digits_          = true;
    bool symbols_         = true;
    bool exclude_similar_ = true;
};

struct PasswordResult {
    std::string password_;
    int length_;
    double entropy_;
    std::string strength_;
};

/// Returns false and fills error_message when the config is invalid.
bool validate_config(const PasswordConfig& config, std::string& error_message);

/// Generates one password; throws std::invalid_argument on invalid config.
PasswordResult generate(const PasswordConfig& config);

/// Generates count passwords; throws std::invalid_argument on bad input.
std::vector<PasswordResult> generate_batch(const PasswordConfig& config, int count);

}  // namespace ip_server
