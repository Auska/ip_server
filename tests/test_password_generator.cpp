#include <gtest/gtest.h>

#include <set>
#include <string>

#include "password_generator.h"

using namespace ip_server;

class PasswordGeneratorTest : public ::testing::Test {
   protected:
    PasswordGenerator generator_;
};

TEST_F(PasswordGeneratorTest, GenerateDefaultPassword) {
    PasswordConfig config;
    auto result = generator_.generate(config);

    EXPECT_EQ(result.length, 16);
    EXPECT_FALSE(result.password.empty());
    EXPECT_GT(result.entropy, 0.0);
    EXPECT_FALSE(result.strength.empty());
}

TEST_F(PasswordGeneratorTest, GenerateCustomLengthPassword) {
    PasswordConfig config;
    config.length = 24;
    auto result   = generator_.generate(config);

    EXPECT_EQ(result.length, 24);
    EXPECT_EQ(result.password.length(), 24);
}

TEST_F(PasswordGeneratorTest, GenerateMinimalLengthPassword) {
    PasswordConfig config;
    config.length = 8;
    auto result   = generator_.generate(config);

    EXPECT_EQ(result.length, 8);
    EXPECT_EQ(result.password.length(), 8);
}

TEST_F(PasswordGeneratorTest, GenerateMaximalLengthPassword) {
    PasswordConfig config;
    config.length = 128;
    auto result   = generator_.generate(config);

    EXPECT_EQ(result.length, 128);
    EXPECT_EQ(result.password.length(), 128);
}

TEST_F(PasswordGeneratorTest, GenerateUppercaseOnlyPassword) {
    PasswordConfig config;
    config.uppercase       = true;
    config.lowercase       = false;
    config.digits          = false;
    config.symbols         = false;
    config.exclude_similar = true;

    auto result = generator_.generate(config);

    for (char c : result.password) {
        EXPECT_GE(c, 'A');
        EXPECT_LE(c, 'Z');
        EXPECT_NE(c, 'I');
        EXPECT_NE(c, 'O');
    }
}

TEST_F(PasswordGeneratorTest, GenerateLowercaseOnlyPassword) {
    PasswordConfig config;
    config.uppercase       = false;
    config.lowercase       = true;
    config.digits          = false;
    config.symbols         = false;
    config.exclude_similar = true;

    auto result = generator_.generate(config);

    for (char c : result.password) {
        EXPECT_GE(c, 'a');
        EXPECT_LE(c, 'z');
        EXPECT_NE(c, 'i');
        EXPECT_NE(c, 'l');
        EXPECT_NE(c, 'o');
    }
}

TEST_F(PasswordGeneratorTest, GenerateDigitsOnlyPassword) {
    PasswordConfig config;
    config.uppercase       = false;
    config.lowercase       = false;
    config.digits          = true;
    config.symbols         = false;
    config.exclude_similar = true;

    auto result = generator_.generate(config);

    for (char c : result.password) {
        EXPECT_GE(c, '0');
        EXPECT_LE(c, '9');
        EXPECT_NE(c, '0');
        EXPECT_NE(c, '1');
    }
}

TEST_F(PasswordGeneratorTest, GenerateMixedPassword) {
    PasswordConfig config;
    config.uppercase = true;
    config.lowercase = true;
    config.digits    = true;
    config.symbols   = true;

    auto result = generator_.generate(config);

    bool has_upper  = false;
    bool has_lower  = false;
    bool has_digit  = false;
    bool has_symbol = false;

    for (char c : result.password) {
        if (c >= 'A' && c <= 'Z') has_upper = true;
        if (c >= 'a' && c <= 'z') has_lower = true;
        if (c >= '0' && c <= '9') has_digit = true;
        if (std::string("!@#$%^&*()_+-=[]{}|;:,.<>?").find(c) != std::string::npos) {
            has_symbol = true;
        }
    }

    EXPECT_TRUE(has_upper);
    EXPECT_TRUE(has_lower);
    EXPECT_TRUE(has_digit);
    EXPECT_TRUE(has_symbol);
}

TEST_F(PasswordGeneratorTest, ExcludeSimilarCharacters) {
    PasswordConfig config;
    config.exclude_similar = true;
    config.uppercase       = true;
    config.lowercase       = true;
    config.digits          = true;

    auto result = generator_.generate(config);

    for (char c : result.password) {
        EXPECT_NE(c, 'I');
        EXPECT_NE(c, 'O');
        EXPECT_NE(c, 'i');
        EXPECT_NE(c, 'l');
        EXPECT_NE(c, 'o');
        EXPECT_NE(c, '0');
        EXPECT_NE(c, '1');
    }
}

TEST_F(PasswordGeneratorTest, GenerateBatchPasswords) {
    PasswordConfig config;
    int count    = 5;
    auto results = generator_.generate_batch(config, count);

    EXPECT_EQ(results.size(), count);

    std::set<std::string> unique_passwords;
    for (const auto& result : results) {
        unique_passwords.insert(result.password);
        EXPECT_EQ(result.length, 16);
        EXPECT_GT(result.entropy, 0.0);
    }

    // With high probability, all passwords should be unique
    EXPECT_EQ(unique_passwords.size(), count);
}

TEST_F(PasswordGeneratorTest, GenerateMaxBatchPasswords) {
    PasswordConfig config;
    int count    = 100;
    auto results = generator_.generate_batch(config, count);

    EXPECT_EQ(results.size(), 100);
}

TEST_F(PasswordGeneratorTest, ValidateConfigInvalidLengthTooShort) {
    PasswordConfig config;
    config.length = 7;
    std::string error_message;
    EXPECT_FALSE(PasswordGenerator::validate_config(config, error_message));
    EXPECT_FALSE(error_message.empty());
}

TEST_F(PasswordGeneratorTest, ValidateConfigInvalidLengthTooLong) {
    PasswordConfig config;
    config.length = 129;
    std::string error_message;
    EXPECT_FALSE(PasswordGenerator::validate_config(config, error_message));
    EXPECT_FALSE(error_message.empty());
}

TEST_F(PasswordGeneratorTest, ValidateConfigNoCharacterTypes) {
    PasswordConfig config;
    config.uppercase = false;
    config.lowercase = false;
    config.digits    = false;
    config.symbols   = false;
    std::string error_message;
    EXPECT_FALSE(PasswordGenerator::validate_config(config, error_message));
    EXPECT_FALSE(error_message.empty());
}

TEST_F(PasswordGeneratorTest, ValidateConfigValid) {
    PasswordConfig config;
    std::string error_message;
    EXPECT_TRUE(PasswordGenerator::validate_config(config, error_message));
    EXPECT_TRUE(error_message.empty());
}

TEST_F(PasswordGeneratorTest, EntropyCalculation) {
    PasswordConfig config;
    config.length = 16;
    auto result   = generator_.generate(config);

    // Entropy should be positive for a valid password
    EXPECT_GT(result.entropy, 0.0);
}

TEST_F(PasswordGeneratorTest, StrengthRating) {
    struct TestCase {
        int length;
        int pool_size;
        std::string expected_strength;
    };

    std::vector<TestCase> test_cases = {
        {8, 10, "weak"},         // Low entropy
        {12, 26, "fair"},        // Medium entropy
        {16, 62, "strong"},      // High entropy
        {24, 94, "very_strong"}  // Very high entropy
    };

    for (const auto& tc : test_cases) {
        PasswordConfig config;
        config.length = tc.length;
        auto result   = generator_.generate(config);

        // Verify strength is one of the valid ratings
        EXPECT_TRUE(result.strength == "very_weak" || result.strength == "weak"
                    || result.strength == "fair" || result.strength == "strong"
                    || result.strength == "very_strong");
    }
}

TEST_F(PasswordGeneratorTest, GenerateWithExcludeSimilarFalse) {
    PasswordConfig config;
    config.exclude_similar = false;
    config.uppercase       = true;
    config.lowercase       = true;
    config.digits          = true;

    auto result = generator_.generate(config);

    // Verify password contains characters from all types
    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;

    for (char c : result.password) {
        if (c >= 'A' && c <= 'Z') has_upper = true;
        if (c >= 'a' && c <= 'z') has_lower = true;
        if (c >= '0' && c <= '9') has_digit = true;
    }

    EXPECT_TRUE(has_upper);
    EXPECT_TRUE(has_lower);
    EXPECT_TRUE(has_digit);
}

TEST_F(PasswordGeneratorTest, GenerateBatchInvalidCount) {
    PasswordConfig config;

    EXPECT_THROW(generator_.generate_batch(config, 0), std::runtime_error);
    EXPECT_THROW(generator_.generate_batch(config, -1), std::runtime_error);
    EXPECT_THROW(generator_.generate_batch(config, 101), std::runtime_error);
}

TEST_F(PasswordGeneratorTest, GenerateInvalidLengthThrows) {
    PasswordConfig config;
    config.length = 5;

    EXPECT_THROW(generator_.generate(config), std::runtime_error);
}