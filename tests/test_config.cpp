#include <gtest/gtest.h>

#include "config.h"

using namespace ip_server;

class ConfigTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Reset to default before each test
        default_config = ServerConfig{};
    }

    ServerConfig default_config;
};

TEST_F(ConfigTest, DefaultConfigValues) {
    EXPECT_EQ(default_config.host_, "0.0.0.0");
    EXPECT_EQ(default_config.port_, 8080);
    EXPECT_EQ(default_config.thread_pool_size_, 4);
}

TEST_F(ConfigTest, ParseNoArguments) {
    // Set up a temporary config path that doesn't exist to avoid loading existing config
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.host_, "0.0.0.0");
    EXPECT_EQ(config.port_, 8080);
}

TEST_F(ConfigTest, ParseHostArgument) {
    // Set up a temporary config path that doesn't exist to avoid loading existing config
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--host",
                          "127.0.0.1"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.host_, "127.0.0.1");
    EXPECT_EQ(config.port_, 8080);
}

TEST_F(ConfigTest, ParsePortArgument) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--port", "9000"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.host_, "0.0.0.0");
    EXPECT_EQ(config.port_, 9000);
}

TEST_F(ConfigTest, ParseCityDbArgument) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--city-db",
                          "/path/to/city.mmdb"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.city_db_path_, "/path/to/city.mmdb");
}

TEST_F(ConfigTest, ParseAsnDbArgument) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--asn-db",
                          "/path/to/asn.mmdb"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.asn_db_path_, "/path/to/asn.mmdb");
}

TEST_F(ConfigTest, ParseThreadsArgument) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--threads", "8"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.thread_pool_size_, 8);
}

TEST_F(ConfigTest, ParseMultipleArguments) {
    const char* argv[] = {"ip_server", "--config",         "/nonexistent/config.json",
                          "--host",    "192.168.1.1",      "--port",
                          "8080",      "--city-db",        "/custom/city.mmdb",
                          "--asn-db",  "/custom/asn.mmdb", "--threads",
                          "16"};
    int argc           = 13;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.host_, "192.168.1.1");
    EXPECT_EQ(config.port_, 8080);
    EXPECT_EQ(config.city_db_path_, "/custom/city.mmdb");
    EXPECT_EQ(config.asn_db_path_, "/custom/asn.mmdb");
    EXPECT_EQ(config.thread_pool_size_, 16);
}

TEST_F(ConfigTest, InvalidPortNumber) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--port", "invalid"};
    int argc           = 5;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, InvalidThreadCount) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--threads",
                          "invalid"};
    int argc           = 5;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ParseEnableRateLimiter) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json",
                          "--enable-rate-limiter", "false"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_FALSE(config.enable_rate_limiter_);
}

TEST_F(ConfigTest, ParseMaxRequestsPerMinute) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json",
                          "--max-requests-per-minute", "200"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.max_requests_per_minute_, 200);
}

TEST_F(ConfigTest, ParseMaxBatchSize) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--max-batch-size",
                          "50"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.max_batch_size_, 50);
}

TEST_F(ConfigTest, ParseAllNewParameters) {
    const char* argv[] = {"ip_server",
                          "--config",
                          "/nonexistent/config.json",
                          "--enable-rate-limiter",
                          "true",
                          "--max-requests-per-minute",
                          "150",
                          "--max-batch-size",
                          "75"};
    int argc           = 9;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_TRUE(config.enable_rate_limiter_);
    EXPECT_EQ(config.max_requests_per_minute_, 150);
    EXPECT_EQ(config.max_batch_size_, 75);
}

TEST_F(ConfigTest, DefaultNewParameters) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_TRUE(config.enable_rate_limiter_);
    EXPECT_EQ(config.max_requests_per_minute_, 100);
    EXPECT_EQ(config.max_batch_size_, 100);
}

TEST_F(ConfigTest, InvalidMaxRequestsPerMinute) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json",
                          "--max-requests-per-minute", "invalid"};
    int argc           = 5;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, InvalidMaxBatchSize) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--max-batch-size",
                          "invalid"};
    int argc           = 5;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, RateLimiterDisabledWithZeroRequests) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json",
                          "--enable-rate-limiter", "false"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_FALSE(config.enable_rate_limiter_);
}

TEST_F(ConfigTest, ValidatePortRange) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--port", "0"};
    int argc           = 5;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ValidPortRange) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--port", "65535"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));
    EXPECT_EQ(config.port_, 65535);
}

TEST_F(ConfigTest, ValidateThreadPoolSizeTooSmall) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--threads", "0"};
    int argc           = 5;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ValidateThreadPoolSizeTooLarge) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--threads", "100"};
    int argc           = 5;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ValidThreadPoolSize) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--threads", "64"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));
    EXPECT_EQ(config.thread_pool_size_, 64);
}

TEST_F(ConfigTest, ValidateMaxRequestsPerMinuteTooSmall) {
    const char* argv[] = {"ip_server",
                          "--config",
                          "/nonexistent/config.json",
                          "--enable-rate-limiter",
                          "true",
                          "--max-requests-per-minute",
                          "0"};
    int argc           = 7;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ValidateMaxRequestsPerMinuteTooLarge) {
    const char* argv[] = {"ip_server",
                          "--config",
                          "/nonexistent/config.json",
                          "--enable-rate-limiter",
                          "true",
                          "--max-requests-per-minute",
                          "20000"};
    int argc           = 7;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ValidateMaxBatchSizeTooSmall) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--max-batch-size",
                          "0"};
    int argc           = 5;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ValidateMaxBatchSizeTooLarge) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--max-batch-size",
                          "2000"};
    int argc           = 5;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ParseEnableApiAuth) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--enable-api-auth",
                          "true"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_TRUE(config.enable_api_auth_);
}

TEST_F(ConfigTest, ParseApiKeysFile) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--api-keys-file",
                          "/path/to/keys.txt"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.api_keys_file_, "/path/to/keys.txt");
}

TEST_F(ConfigTest, ParseDefaultApiKey) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json", "--default-api-key",
                          "my_secret_key"};
    int argc           = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.default_api_key_, "my_secret_key");
}

TEST_F(ConfigTest, DefaultApiAuthDisabled) {
    const char* argv[] = {"ip_server", "--config", "/nonexistent/config.json"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_FALSE(config.enable_api_auth_);
    EXPECT_TRUE(config.api_keys_file_.empty());
    EXPECT_TRUE(config.default_api_key_.empty());
}