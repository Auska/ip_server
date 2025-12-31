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
    EXPECT_TRUE(default_config.use_xdg);
    EXPECT_EQ(default_config.thread_pool_size, 4);
}

TEST_F(ConfigTest, ParseNoArguments) {
    const char* argv[] = {"ip_server"};
    int argc = 1;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.host, "0.0.0.0");
    EXPECT_EQ(config.port, 8080);
    EXPECT_TRUE(config.use_xdg);
    EXPECT_FALSE(config.city_db_path.empty());
    EXPECT_FALSE(config.asn_db_path.empty());
}

TEST_F(ConfigTest, ParseHostArgument) {
    const char* argv[] = {"ip_server", "--host", "127.0.0.1"};
    int argc = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.host, "127.0.0.1");
    EXPECT_EQ(config.port, 8080);
}

TEST_F(ConfigTest, ParsePortArgument) {
    const char* argv[] = {"ip_server", "--port", "9000"};
    int argc = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.host, "0.0.0.0");
    EXPECT_EQ(config.port, 9000);
}

TEST_F(ConfigTest, ParseCityDbArgument) {
    const char* argv[] = {"ip_server", "--city-db", "/path/to/city.mmdb"};
    int argc = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.city_db_path, "/path/to/city.mmdb");
    EXPECT_FALSE(config.use_xdg);
}

TEST_F(ConfigTest, ParseAsnDbArgument) {
    const char* argv[] = {"ip_server", "--asn-db", "/path/to/asn.mmdb"};
    int argc = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.asn_db_path, "/path/to/asn.mmdb");
    EXPECT_FALSE(config.use_xdg);
}

TEST_F(ConfigTest, ParseThreadsArgument) {
    const char* argv[] = {"ip_server", "--threads", "8"};
    int argc = 3;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.thread_pool_size, 8);
}

TEST_F(ConfigTest, ParseMultipleArguments) {
    const char* argv[] = {
        "ip_server",
        "--host", "192.168.1.1",
        "--port", "8080",
        "--city-db", "/custom/city.mmdb",
        "--asn-db", "/custom/asn.mmdb",
        "--threads", "16"
    };
    int argc = 11;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.host, "192.168.1.1");
    EXPECT_EQ(config.port, 8080);
    EXPECT_EQ(config.city_db_path, "/custom/city.mmdb");
    EXPECT_EQ(config.asn_db_path, "/custom/asn.mmdb");
    EXPECT_EQ(config.thread_pool_size, 16);
    EXPECT_FALSE(config.use_xdg);
}

TEST_F(ConfigTest, InvalidPortNumber) {
    const char* argv[] = {"ip_server", "--port", "invalid"};
    int argc = 3;

    EXPECT_THROW(
        ConfigParser::parse(argc, const_cast<char**>(argv)),
        std::runtime_error
    );
}

TEST_F(ConfigTest, InvalidThreadCount) {
    const char* argv[] = {"ip_server", "--threads", "invalid"};
    int argc = 3;

    EXPECT_THROW(
        ConfigParser::parse(argc, const_cast<char**>(argv)),
        std::runtime_error
    );
}