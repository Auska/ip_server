#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "auth.h"

using namespace ip_server;

class APIAuthTest : public ::testing::Test {
   protected:
    void SetUp() override { auth = std::make_unique<APIAuth>(); }

    void TearDown() override {
        // Clean up test files
        std::filesystem::remove("test_api_keys.txt");
    }

    std::unique_ptr<APIAuth> auth;
};

TEST_F(APIAuthTest, AddAndValidateKey) {
    std::string key = "test_api_key_123";

    auth->add_key(key);
    EXPECT_TRUE(auth->is_valid(key));
    EXPECT_EQ(auth->key_count(), 1);
}

TEST_F(APIAuthTest, InvalidKeyRejected) {
    std::string valid_key   = "valid_key_123";
    std::string invalid_key = "invalid_key_456";

    auth->add_key(valid_key);
    EXPECT_TRUE(auth->is_valid(valid_key));
    EXPECT_FALSE(auth->is_valid(invalid_key));
}

TEST_F(APIAuthTest, MultipleKeys) {
    auth->add_key("key1");
    auth->add_key("key2");
    auth->add_key("key3");

    EXPECT_TRUE(auth->is_valid("key1"));
    EXPECT_TRUE(auth->is_valid("key2"));
    EXPECT_TRUE(auth->is_valid("key3"));
    EXPECT_FALSE(auth->is_valid("key4"));
    EXPECT_EQ(auth->key_count(), 3);
}

TEST_F(APIAuthTest, EmptyKeyIgnored) {
    auth->add_key("");
    EXPECT_EQ(auth->key_count(), 0);
}

TEST_F(APIAuthTest, DuplicateKeyIgnored) {
    std::string key = "duplicate_key";

    auth->add_key(key);
    auth->add_key(key);

    EXPECT_EQ(auth->key_count(), 1);
}

TEST_F(APIAuthTest, LoadKeysFromFile) {
    // Create test file
    std::ofstream file("test_api_keys.txt");
    file << "# This is a comment\n";
    file << "key1\n";
    file << "key2\n";
    file << "\n";  // Empty line
    file << "key3\n";
    file.close();

    bool result = auth->load_keys_from_file("test_api_keys.txt");

    EXPECT_TRUE(result);
    EXPECT_EQ(auth->key_count(), 3);
    EXPECT_TRUE(auth->is_valid("key1"));
    EXPECT_TRUE(auth->is_valid("key2"));
    EXPECT_TRUE(auth->is_valid("key3"));
}

TEST_F(APIAuthTest, LoadKeysFromNonExistentFile) {
    bool result = auth->load_keys_from_file("nonexistent_file.txt");
    EXPECT_FALSE(result);
}

TEST_F(APIAuthTest, LoadKeysFromFileWithWhitespace) {
    // Create test file with whitespace
    std::ofstream file("test_api_keys.txt");
    file << "  key1  \n";
    file << "\tkey2\t\n";
    file << "key3\n";
    file.close();

    bool result = auth->load_keys_from_file("test_api_keys.txt");

    EXPECT_TRUE(result);
    EXPECT_EQ(auth->key_count(), 3);
    EXPECT_TRUE(auth->is_valid("key1"));
    EXPECT_TRUE(auth->is_valid("key2"));
    EXPECT_TRUE(auth->is_valid("key3"));
}

TEST_F(APIAuthTest, LoadKeysFromFileClearsExistingKeys) {
    // Add some keys first
    auth->add_key("old_key1");
    auth->add_key("old_key2");
    EXPECT_EQ(auth->key_count(), 2);

    // Create test file
    std::ofstream file("test_api_keys.txt");
    file << "new_key1\n";
    file << "new_key2\n";
    file.close();

    auth->load_keys_from_file("test_api_keys.txt");

    EXPECT_EQ(auth->key_count(), 2);
    EXPECT_FALSE(auth->is_valid("old_key1"));
    EXPECT_FALSE(auth->is_valid("old_key2"));
    EXPECT_TRUE(auth->is_valid("new_key1"));
    EXPECT_TRUE(auth->is_valid("new_key2"));
}

TEST_F(APIAuthTest, LongApiKey) {
    std::string long_key(1000, 'a');  // 1000 character key

    auth->add_key(long_key);
    EXPECT_TRUE(auth->is_valid(long_key));
    EXPECT_EQ(auth->key_count(), 1);
}

TEST_F(APIAuthTest, SpecialCharactersInKey) {
    std::string key = "key_with_special_chars_!@#$%^&*()";

    auth->add_key(key);
    EXPECT_TRUE(auth->is_valid(key));
}

TEST_F(APIAuthTest, EmptyFile) {
    // Create empty file
    std::ofstream file("test_api_keys.txt");
    file.close();

    bool result = auth->load_keys_from_file("test_api_keys.txt");

    EXPECT_TRUE(result);
    EXPECT_EQ(auth->key_count(), 0);
}

TEST_F(APIAuthTest, FileWithOnlyComments) {
    // Create file with only comments
    std::ofstream file("test_api_keys.txt");
    file << "# Comment 1\n";
    file << "# Comment 2\n";
    file << "# Comment 3\n";
    file.close();

    bool result = auth->load_keys_from_file("test_api_keys.txt");

    EXPECT_TRUE(result);
    EXPECT_EQ(auth->key_count(), 0);
}

// ─── Edge-case tests ──────────────────────────────────────────────

TEST_F(APIAuthTest, DuplicateKeysInFile) {
    std::ofstream file("test_dup_api_keys.txt");
    file << "key-one\n";
    file << "key-one\n";
    file << "key-two\n";
    file.close();

    bool result = auth->load_keys_from_file("test_dup_api_keys.txt");

    EXPECT_TRUE(result);
    EXPECT_EQ(auth->key_count(), 2);
    std::filesystem::remove("test_dup_api_keys.txt");
}

TEST_F(APIAuthTest, KeyWithWhitespaceOnly) {
    std::ofstream file("test_ws_api_keys.txt");
    file << "   \n";
    file << "\t\n";
    file << "\n";
    file.close();

    bool result = auth->load_keys_from_file("test_ws_api_keys.txt");

    EXPECT_TRUE(result);
    EXPECT_EQ(auth->key_count(), 0);
    std::filesystem::remove("test_ws_api_keys.txt");
}

TEST_F(APIAuthTest, ConcurrentAddAndValidate) {
    auto concurrent_auth = std::make_unique<APIAuth>();

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&, i]() {
            std::string key = "concurrent-key-" + std::to_string(i);
            concurrent_auth->add_key(key);
            EXPECT_TRUE(concurrent_auth->is_valid(key));
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(concurrent_auth->key_count(), 10);
}

TEST_F(APIAuthTest, BinaryDataAsKey) {
    // Binary-like characters (without embedded null, which C-string literals can't represent)
    std::string binary_key = "\x01\x02\x03\xFF\xFEtest-key";
    EXPECT_NO_THROW(auth->add_key(binary_key));
    EXPECT_TRUE(auth->is_valid(binary_key));
}