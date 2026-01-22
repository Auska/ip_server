#include <gtest/gtest.h>

#include "config.h"

using namespace ip_server;

class ConfigTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Reset to default before each test
        default_config = ConfigParser::default_config();
    }

    ServerConfig default_config;
};

TEST_F(ConfigTest, DefaultConfigValues) {
    EXPECT_EQ(default_config.host, "0.0.0.0");
    EXPECT_EQ(default_config.port, 8080);
    EXPECT_EQ(default_config.thread_pool_size, 4);
}

TEST_F(ConfigTest, ParseNoArguments) {
    const char* argv[] = {"ip_server"};
    int argc           = 1;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.host, "0.0.0.0");
    EXPECT_EQ(config.port, 8080);
}

TEST_F(ConfigTest, ParseHostArgument) {
    const char* argv[] = {"ip_server", "--host", "127.0.0.1"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.host, "127.0.0.1");
    EXPECT_EQ(config.port, 8080);
}

TEST_F(ConfigTest, ParsePortArgument) {
    const char* argv[] = {"ip_server", "--port", "9000"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.host, "0.0.0.0");
    EXPECT_EQ(config.port, 9000);
}

TEST_F(ConfigTest, ParseCityDbArgument) {
    const char* argv[] = {"ip_server", "--city-db", "/path/to/city.mmdb"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.city_db_path, "/path/to/city.mmdb");
}

TEST_F(ConfigTest, ParseAsnDbArgument) {
    const char* argv[] = {"ip_server", "--asn-db", "/path/to/asn.mmdb"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.asn_db_path, "/path/to/asn.mmdb");
}

TEST_F(ConfigTest, ParseThreadsArgument) {
    const char* argv[] = {"ip_server", "--threads", "8"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.thread_pool_size, 8);
}

TEST_F(ConfigTest, ParseMultipleArguments) {
    const char* argv[] =
        {"ip_server",         "--host",   "192.168.1.1",      "--port",    "8080", "--city-db",
         "/custom/city.mmdb", "--asn-db", "/custom/asn.mmdb", "--threads", "16"};
    int argc = 11;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.host, "192.168.1.1");
    EXPECT_EQ(config.port, 8080);
    EXPECT_EQ(config.city_db_path, "/custom/city.mmdb");
    EXPECT_EQ(config.asn_db_path, "/custom/asn.mmdb");
    EXPECT_EQ(config.thread_pool_size, 16);
}

TEST_F(ConfigTest, InvalidPortNumber) {
    const char* argv[] = {"ip_server", "--port", "invalid"};
    int argc           = 3;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, InvalidThreadCount) {
    const char* argv[] = {"ip_server", "--threads", "invalid"};
    int argc           = 3;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ParseEnableRateLimiter) {
    const char* argv[] = {"ip_server", "--enable-rate-limiter", "false"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_FALSE(config.enable_rate_limiter);
}

TEST_F(ConfigTest, ParseMaxRequestsPerMinute) {
    const char* argv[] = {"ip_server", "--max-requests-per-minute", "200"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.max_requests_per_minute, 200);
}

TEST_F(ConfigTest, ParseMaxBatchSize) {
    const char* argv[] = {"ip_server", "--max-batch-size", "50"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.max_batch_size, 50);
}

TEST_F(ConfigTest, ParseAllNewParameters) {
    const char* argv[] = {"ip_server", "--enable-rate-limiter", "true", "--max-requests-per-minute",
                          "150",       "--max-batch-size",      "75"};
    int argc           = 7;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_TRUE(config.enable_rate_limiter);
    EXPECT_EQ(config.max_requests_per_minute, 150);
    EXPECT_EQ(config.max_batch_size, 75);
}

TEST_F(ConfigTest, DefaultNewParameters) {
    const char* argv[] = {"ip_server"};
    int argc           = 1;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_TRUE(config.enable_rate_limiter);
    EXPECT_EQ(config.max_requests_per_minute, 100);
    EXPECT_EQ(config.max_batch_size, 100);
}

TEST_F(ConfigTest, InvalidMaxRequestsPerMinute) {
    const char* argv[] = {"ip_server", "--max-requests-per-minute", "invalid"};
    int argc           = 3;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, InvalidMaxBatchSize) {
    const char* argv[] = {"ip_server", "--max-batch-size", "invalid"};
    int argc           = 3;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, RateLimiterDisabledWithZeroRequests) {
    const char* argv[] = {"ip_server", "--enable-rate-limiter", "false"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_FALSE(config.enable_rate_limiter);
}

TEST_F(ConfigTest, ValidatePortRange) {
    const char* argv[] = {"ip_server", "--port", "0"};
    int argc           = 3;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ValidPortRange) {
    const char* argv[] = {"ip_server", "--port", "65535"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));
    EXPECT_EQ(config.port, 65535);
}

TEST_F(ConfigTest, ValidateThreadPoolSizeTooSmall) {
    const char* argv[] = {"ip_server", "--threads", "0"};
    int argc           = 3;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ValidateThreadPoolSizeTooLarge) {
    const char* argv[] = {"ip_server", "--threads", "100"};
    int argc           = 3;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ValidThreadPoolSize) {
    const char* argv[] = {"ip_server", "--threads", "64"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));
    EXPECT_EQ(config.thread_pool_size, 64);
}

TEST_F(ConfigTest, ValidateMaxRequestsPerMinuteTooSmall) {
    const char* argv[] = {"ip_server", "--enable-rate-limiter", "true", "--max-requests-per-minute",
                          "0"};
    int argc           = 5;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ValidateMaxRequestsPerMinuteTooLarge) {
    const char* argv[] = {"ip_server", "--enable-rate-limiter", "true", "--max-requests-per-minute",
                          "20000"};
    int argc           = 5;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ValidateMaxBatchSizeTooSmall) {
    const char* argv[] = {"ip_server", "--max-batch-size", "0"};
    int argc           = 3;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ValidateMaxBatchSizeTooLarge) {
    const char* argv[] = {"ip_server", "--max-batch-size", "2000"};
    int argc           = 3;

    EXPECT_THROW(ConfigParser::parse(argc, const_cast<char**>(argv)), std::runtime_error);
}

TEST_F(ConfigTest, ParseEnableApiAuth) {
    const char* argv[] = {"ip_server", "--enable-api-auth", "true"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_TRUE(config.enable_api_auth);
}

TEST_F(ConfigTest, ParseApiKeysFile) {
    const char* argv[] = {"ip_server", "--api-keys-file", "/path/to/keys.txt"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.api_keys_file, "/path/to/keys.txt");
}

TEST_F(ConfigTest, ParseDefaultApiKey) {
    const char* argv[] = {"ip_server", "--default-api-key", "my_secret_key"};
    int argc           = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.default_api_key, "my_secret_key");
}

TEST_F(ConfigTest, DefaultApiAuthDisabled) {
    const char* argv[] = {"ip_server"};
    int argc           = 1;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_FALSE(config.enable_api_auth);
    EXPECT_TRUE(config.api_keys_file.empty());
    EXPECT_TRUE(config.default_api_key.empty());
}