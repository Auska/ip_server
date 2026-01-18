#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "auth.h"

using namespace ip_server;

class APIAuthTest : public ::testing::Test {
   protected:
    void SetUp() override { auth = std::make_unique<APIAuth>(true); }

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

TEST_F(APIAuthTest, RemoveKey) {
    std::string key = "test_key_123";

    auth->add_key(key);
    EXPECT_TRUE(auth->is_valid(key));
    EXPECT_EQ(auth->key_count(), 1);

    auth->remove_key(key);
    EXPECT_FALSE(auth->is_valid(key));
    EXPECT_EQ(auth->key_count(), 0);
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

TEST_F(APIAuthTest, AuthDisabled) {
    auto disabled_auth = std::make_unique<APIAuth>(false);

    // When auth is disabled, all keys should be valid
    EXPECT_TRUE(disabled_auth->is_valid("any_key"));
    EXPECT_TRUE(disabled_auth->is_valid(""));
    EXPECT_FALSE(disabled_auth->is_enabled());
}

TEST_F(APIAuthTest, AuthEnabled) {
    EXPECT_TRUE(auth->is_enabled());
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