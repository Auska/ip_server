#pragma once

#include <string>
#include <vector>

namespace ip_server {

struct PasswordConfig {
    int length           = 16;
    bool uppercase       = true;
    bool lowercase       = true;
    bool digits          = true;
    bool symbols         = true;
    bool exclude_similar = true;
};

struct PasswordResult {
    std::string password;
    int length;
    double entropy;
    std::string strength;
};

class PasswordGenerator {
   public:
    PasswordGenerator();
    ~PasswordGenerator() = default;

    PasswordGenerator(const PasswordGenerator&)            = delete;
    PasswordGenerator& operator=(const PasswordGenerator&) = delete;

    PasswordGenerator(PasswordGenerator&&) noexcept            = default;
    PasswordGenerator& operator=(PasswordGenerator&&) noexcept = default;

    static PasswordResult generate(const PasswordConfig& config);
    static std::vector<PasswordResult> generate_batch(const PasswordConfig& config, int count);

    static bool validate_config(const PasswordConfig& config, std::string& error_message);

   private:
    static std::string build_character_pool(const PasswordConfig& config) ;
    static double calculate_entropy(const std::string& password, int pool_size);
    static std::string get_strength_rating(double entropy);

    // Character sets
    static constexpr const char* UPPERCASE = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static constexpr const char* LOWERCASE = "abcdefghijklmnopqrstuvwxyz";
    static constexpr const char* DIGITS    = "0123456789";
    static constexpr const char* SYMBOLS   = "!@#$%^&*()_+-=[]{}|;:,.<>?";

    // Confusing characters to exclude
    static constexpr const char* CONFUSING_UPPER  = "IO";
    static constexpr const char* CONFUSING_LOWER  = "ilo";
    static constexpr const char* CONFUSING_DIGITS = "01";
};

}  // namespace ip_server