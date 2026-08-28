#include <gtest/gtest.h>

#include <set>
#include <stdexcept>
#include <string>

#include "password_generator.h"

using namespace ip_server;

class PasswordGeneratorTest : public ::testing::Test {};

TEST_F(PasswordGeneratorTest, GenerateDefaultPassword) {
    PasswordConfig config;
    auto result = generate(config);

    EXPECT_EQ(result.length_, 16);
    EXPECT_FALSE(result.password_.empty());
    EXPECT_GT(result.entropy_, 0.0);
    EXPECT_FALSE(result.strength_.empty());
}

TEST_F(PasswordGeneratorTest, GenerateCustomLengthPassword) {
    PasswordConfig config;
    config.length_ = 24;
    auto result    = generate(config);

    EXPECT_EQ(result.length_, 24);
    EXPECT_EQ(result.password_.length(), 24);
}

TEST_F(PasswordGeneratorTest, GenerateMinimalLengthPassword) {
    PasswordConfig config;
    config.length_ = 8;
    auto result    = generate(config);

    EXPECT_EQ(result.length_, 8);
    EXPECT_EQ(result.password_.length(), 8);
}

TEST_F(PasswordGeneratorTest, GenerateMaximalLengthPassword) {
    PasswordConfig config;
    config.length_ = 128;
    auto result    = generate(config);

    EXPECT_EQ(result.length_, 128);
    EXPECT_EQ(result.password_.length(), 128);
}

TEST_F(PasswordGeneratorTest, GenerateUppercaseOnlyPassword) {
    PasswordConfig config;
    config.uppercase_       = true;
    config.lowercase_       = false;
    config.digits_          = false;
    config.symbols_         = false;
    config.exclude_similar_ = true;

    auto result = generate(config);

    for (char c : result.password_) {
        EXPECT_GE(c, 'A');
        EXPECT_LE(c, 'Z');
        EXPECT_NE(c, 'I');
        EXPECT_NE(c, 'O');
    }
}

TEST_F(PasswordGeneratorTest, GenerateLowercaseOnlyPassword) {
    PasswordConfig config;
    config.uppercase_       = false;
    config.lowercase_       = true;
    config.digits_          = false;
    config.symbols_         = false;
    config.exclude_similar_ = true;

    auto result = generate(config);

    for (char c : result.password_) {
        EXPECT_GE(c, 'a');
        EXPECT_LE(c, 'z');
        EXPECT_NE(c, 'i');
        EXPECT_NE(c, 'l');
        EXPECT_NE(c, 'o');
    }
}

TEST_F(PasswordGeneratorTest, GenerateDigitsOnlyPassword) {
    PasswordConfig config;
    config.uppercase_       = false;
    config.lowercase_       = false;
    config.digits_          = true;
    config.symbols_         = false;
    config.exclude_similar_ = true;

    auto result = generate(config);

    for (char c : result.password_) {
        EXPECT_GE(c, '0');
        EXPECT_LE(c, '9');
        EXPECT_NE(c, '0');
        EXPECT_NE(c, '1');
    }
}

TEST_F(PasswordGeneratorTest, GenerateMixedPassword) {
    PasswordConfig config;
    config.uppercase_ = true;
    config.lowercase_ = true;
    config.digits_    = true;
    config.symbols_   = true;

    auto result = generate(config);

    bool has_upper  = false;
    bool has_lower  = false;
    bool has_digit  = false;
    bool has_symbol = false;

    for (char c : result.password_) {
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
    config.exclude_similar_ = true;
    config.uppercase_       = true;
    config.lowercase_       = true;
    config.digits_          = true;

    auto result = generate(config);

    for (char c : result.password_) {
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
    auto results = generate_batch(config, count);

    EXPECT_EQ(results.size(), count);

    std::set<std::string> unique_passwords;
    for (const auto& result : results) {
        unique_passwords.insert(result.password_);
        EXPECT_EQ(result.length_, 16);
        EXPECT_GT(result.entropy_, 0.0);
    }

    // With high probability, all passwords should be unique
    EXPECT_EQ(unique_passwords.size(), count);
}

TEST_F(PasswordGeneratorTest, GenerateMaxBatchPasswords) {
    PasswordConfig config;
    int count    = 100;
    auto results = generate_batch(config, count);

    EXPECT_EQ(results.size(), 100);
}

TEST_F(PasswordGeneratorTest, ValidateConfigInvalidLengthTooShort) {
    PasswordConfig config;
    config.length_ = 7;
    std::string error_message;
    EXPECT_FALSE(validate_config(config, error_message));
    EXPECT_FALSE(error_message.empty());
}

TEST_F(PasswordGeneratorTest, ValidateConfigInvalidLengthTooLong) {
    PasswordConfig config;
    config.length_ = 129;
    std::string error_message;
    EXPECT_FALSE(validate_config(config, error_message));
    EXPECT_FALSE(error_message.empty());
}

TEST_F(PasswordGeneratorTest, ValidateConfigNoCharacterTypes) {
    PasswordConfig config;
    config.uppercase_ = false;
    config.lowercase_ = false;
    config.digits_    = false;
    config.symbols_   = false;
    std::string error_message;
    EXPECT_FALSE(validate_config(config, error_message));
    EXPECT_FALSE(error_message.empty());
}

TEST_F(PasswordGeneratorTest, ValidateConfigValid) {
    PasswordConfig config;
    std::string error_message;
    EXPECT_TRUE(validate_config(config, error_message));
    EXPECT_TRUE(error_message.empty());
}

TEST_F(PasswordGeneratorTest, EntropyCalculation) {
    PasswordConfig config;
    config.length_ = 16;
    auto result    = generate(config);

    // Entropy should be positive for a valid password
    EXPECT_GT(result.entropy_, 0.0);
}

TEST_F(PasswordGeneratorTest, StrengthRating) {
    struct TestCase {
        int length_;
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
        config.length_ = tc.length_;
        auto result    = generate(config);

        // Verify strength is one of the valid ratings
        EXPECT_TRUE(result.strength_ == "very_weak" || result.strength_ == "weak"
                    || result.strength_ == "fair" || result.strength_ == "strong"
                    || result.strength_ == "very_strong");
    }
}

TEST_F(PasswordGeneratorTest, GenerateWithExcludeSimilarFalse) {
    PasswordConfig config;
    config.exclude_similar_ = false;
    config.uppercase_       = true;
    config.lowercase_       = true;
    config.digits_          = true;

    auto result = generate(config);

    // Verify password contains characters from all types
    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;

    for (char c : result.password_) {
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

    EXPECT_THROW(generate_batch(config, 0), std::invalid_argument);
    EXPECT_THROW(generate_batch(config, -1), std::invalid_argument);
}

TEST_F(PasswordGeneratorTest, GenerateInvalidLengthThrows) {
    PasswordConfig config;
    config.length_ = 5;

    EXPECT_THROW(generate(config), std::invalid_argument);
}

// ─── Edge-case tests ──────────────────────────────────────────────

TEST_F(PasswordGeneratorTest, LengthBoundaryEight) {
    PasswordConfig config;
    config.length_ = 8;
    auto result    = generate(config);
    EXPECT_EQ(result.length_, 8);
    EXPECT_FALSE(result.password_.empty());
}

TEST_F(PasswordGeneratorTest, LengthBoundaryOneTwentyEight) {
    PasswordConfig config;
    config.length_ = 128;
    auto result    = generate(config);
    EXPECT_EQ(result.length_, 128);
    EXPECT_FALSE(result.password_.empty());
}

TEST_F(PasswordGeneratorTest, SymbolsOnlyPassword) {
    PasswordConfig config;
    config.uppercase_       = false;
    config.lowercase_       = false;
    config.digits_          = false;
    config.symbols_         = true;
    config.exclude_similar_ = false;

    auto result = generate(config);
    EXPECT_EQ(result.length_, 16);

    for (char c : result.password_) {
        bool is_symbol = c == '!' || c == '@' || c == '#' || c == '$' || c == '%' || c == '^'
                         || c == '&' || c == '*' || c == '(' || c == ')' || c == '_' || c == '+'
                         || c == '-' || c == '=' || c == '[' || c == ']' || c == '{' || c == '}'
                         || c == '|' || c == ';' || c == ':' || c == ',' || c == '.' || c == '<'
                         || c == '>' || c == '?';
        EXPECT_TRUE(is_symbol) << "Non-symbol character '" << c
                               << "' found in symbols-only password";
    }
}

TEST_F(PasswordGeneratorTest, ExcludeSimilarReducesPool) {
    PasswordConfig config;
    config.uppercase_       = true;
    config.lowercase_       = false;
    config.digits_          = false;
    config.symbols_         = false;
    config.exclude_similar_ = true;

    // Only uppercase with exclude_similar: pool = 26 - 2 = 24 chars
    auto result = generate(config);
    EXPECT_EQ(result.length_, 16);

    // 'I' and 'O' should not appear (confusable)
    for (char c : result.password_) {
        EXPECT_NE(c, 'I');
        EXPECT_NE(c, 'O');
    }
}

TEST_F(PasswordGeneratorTest, StrengthRatingBoundaries) {
    // Verify strength ratings through the public API with known configurations.
    // entropy = length * log2(pool_size)

    struct TestCase {
        PasswordConfig config;
        std::string expected_strength;
    };

    std::vector<TestCase> cases;

    // Digits only, length 8: pool=10, entropy = 8*log2(10) ≈ 26.6 → very_weak (< 28)
    {
        PasswordConfig c;
        c.length_          = 8;
        c.uppercase_       = false;
        c.lowercase_       = false;
        c.digits_          = true;
        c.symbols_         = false;
        c.exclude_similar_ = true;
        cases.push_back({c, "very_weak"});
    }

    // Lowercase only, length 12: pool=26, entropy = 12*log2(26) ≈ 56.4 → fair (36-60)
    {
        PasswordConfig c;
        c.length_          = 12;
        c.uppercase_       = false;
        c.lowercase_       = true;
        c.digits_          = false;
        c.symbols_         = false;
        c.exclude_similar_ = true;
        cases.push_back({c, "fair"});
    }

    // All types, length 12: pool≈86, entropy = 12*log2(86) ≈ 77.1 → strong (60-80)
    {
        PasswordConfig c;
        c.length_          = 12;
        c.uppercase_       = true;
        c.lowercase_       = true;
        c.digits_          = true;
        c.symbols_         = true;
        c.exclude_similar_ = true;
        cases.push_back({c, "strong"});
    }

    // All types, length 20: pool≈86, entropy ≈ 128 → very_strong (>=80)
    {
        PasswordConfig c;
        c.length_          = 20;
        c.uppercase_       = true;
        c.lowercase_       = true;
        c.digits_          = true;
        c.symbols_         = true;
        c.exclude_similar_ = true;
        cases.push_back({c, "very_strong"});
    }

    for (size_t i = 0; i < cases.size(); ++i) {
        auto result = generate(cases[i].config);
        EXPECT_EQ(result.strength_, cases[i].expected_strength)
            << "Case " << i << " failed: expected " << cases[i].expected_strength << " but got "
            << result.strength_ << " (entropy=" << result.entropy_ << ")";
    }
}

TEST_F(PasswordGeneratorTest, GenerateBatchValidOne) {
    PasswordConfig config;
    auto results = generate_batch(config, 1);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].length_, 16);
}

TEST_F(PasswordGeneratorTest, GenerateBatchUniqueResults) {
    PasswordConfig config;
    config.length_ = 32;  // Long enough to be unique
    auto results   = generate_batch(config, 10);

    std::set<std::string> seen;
    for (const auto& r : results) {
        EXPECT_TRUE(seen.find(r.password_) == seen.end())
            << "Duplicate password found in batch: " << r.password_;
        seen.insert(r.password_);
    }
    EXPECT_EQ(seen.size(), 10);
}