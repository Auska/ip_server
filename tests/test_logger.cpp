#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include "logger.h"

using namespace ip_server;

namespace {

std::filesystem::path log_path(const std::string& name) {
    return std::filesystem::temp_directory_path() / (name + ".log");
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

LogConfig file_config(const std::string& name, std::string rotation = "size",
                      size_t max_size = 10 * 1024 * 1024) {
    LogConfig config;
    config.enable_stdout_       = false;
    config.enable_file_logging_ = true;
    config.log_file_path_       = log_path(name).string();
    config.rotation_type_       = std::move(rotation);
    config.max_file_size_       = max_size;
    return config;
}

}  // namespace

class LoggerTest : public ::testing::Test {
   protected:
    void TearDown() override {
        std::error_code ec;
        for (auto const* name : {"logger_test", "logger_test_rot", "logger_test_rot.1",
                                 "logger_test_none"}) {
            std::filesystem::remove(log_path(name), ec);
        }
        set_log_level("info");
    }
};

TEST_F(LoggerTest, FileLoggingWritesMessages) {
    init_logging(file_config("logger_test"));

    LOG_INFO("logger_test_marker_message");
    spdlog::default_logger()->flush();

    EXPECT_NE(read_file(log_path("logger_test")).find("logger_test_marker_message"),
              std::string::npos);
}

TEST_F(LoggerTest, LevelFiltering) {
    init_logging(file_config("logger_test"));
    set_log_level("err");

    LOG_INFO("should_not_appear");
    LOG_ERROR("should_appear");
    spdlog::default_logger()->flush();

    auto const content = read_file(log_path("logger_test"));
    EXPECT_EQ(content.find("should_not_appear"), std::string::npos);
    EXPECT_NE(content.find("should_appear"), std::string::npos);
}

TEST_F(LoggerTest, SetLogLevelAcceptsAllSpdlogNames) {
    init_logging(file_config("logger_test"));

    for (auto const& [name, expected] :
         {std::pair<const char*, spdlog::level::level_enum>{"trace", spdlog::level::trace},
          {"debug", spdlog::level::debug},
          {"info", spdlog::level::info},
          {"warning", spdlog::level::warn},
          {"err", spdlog::level::err},
          {"critical", spdlog::level::critical},
          {"off", spdlog::level::off}}) {
        set_log_level(name);
        EXPECT_EQ(spdlog::default_logger()->level(), expected) << "level name: " << name;
    }
}

TEST_F(LoggerTest, SizeRotation) {
    init_logging(file_config("logger_test_rot", "size", 2048));

    // Each line ~100 bytes; 60 lines > 2KB forces at least one rotation.
    for (int i = 0; i < 60; ++i) {
        LOG_INFO("rotation padding line for the logger test - 0123456789 0123456789 0123456789");
    }
    spdlog::default_logger()->flush();

    EXPECT_TRUE(std::filesystem::exists(log_path("logger_test_rot")));
    EXPECT_TRUE(std::filesystem::exists(log_path("logger_test_rot.1")));
}

TEST_F(LoggerTest, NoRotation) {
    init_logging(file_config("logger_test_none", "none", 2048));

    for (int i = 0; i < 60; ++i) {
        LOG_INFO("rotation padding line for the logger test - 0123456789 0123456789 0123456789");
    }
    spdlog::default_logger()->flush();

    EXPECT_TRUE(std::filesystem::exists(log_path("logger_test_none")));
    EXPECT_FALSE(std::filesystem::exists(log_path("logger_test_none.1")));
}

TEST_F(LoggerTest, NoSinksIsSafe) {
    LogConfig config;
    config.enable_stdout_       = false;
    config.enable_file_logging_ = false;
    init_logging(config);

    std::string msg = "dropped message";
    EXPECT_NO_THROW(LOG_INFO(msg));
}

TEST_F(LoggerTest, SignalSafeLog) {
    EXPECT_NO_THROW(signal_safe_log("SIGTEST"));
}
