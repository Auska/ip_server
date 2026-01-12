#include <gtest/gtest.h>
#include "logger.h"
#include "config.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>

using namespace ip_server;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up test logs directory
        test_log_dir_ = "test_logs";
        if (std::filesystem::exists(test_log_dir_)) {
            std::filesystem::remove_all(test_log_dir_);
        }
        std::filesystem::create_directories(test_log_dir_);
        
        // Save original logger config (keep stdout enabled for tests)
        LogConfig original_config;
        original_config.enable_file_logging = false;
        original_config.enable_stdout = true;
        Logger::instance().set_config(original_config);
    }

    void TearDown() override {
        // Flush any pending logs
        Logger::instance().flush();
        
        // Reset logger to default
        LogConfig default_config;
        default_config.enable_file_logging = false;
        default_config.enable_stdout = true;
        Logger::instance().set_config(default_config);
        
        // Clean up test logs directory at the end
        if (std::filesystem::exists(test_log_dir_)) {
            std::filesystem::remove_all(test_log_dir_);
        }
    }

    std::string get_test_log_path(const std::string& filename = "test.log") {
        return (std::filesystem::path(test_log_dir_) / filename).string();
    }

    // Get backup file path in spdlog format: filename.1.log instead of filename.log.1
    std::string get_backup_path(const std::string& base_path, int index) {
        std::filesystem::path p(base_path);
        std::string stem = p.stem().string();
        std::string ext = p.extension().string();
        return (p.parent_path() / (stem + "." + std::to_string(index) + ext)).string();
    }

    size_t get_file_size(const std::string& filepath) {
        if (!std::filesystem::exists(filepath)) {
            return 0;
        }
        return std::filesystem::file_size(filepath);
    }

    int count_backup_files(const std::string& base_path) {
        int count = 0;
        std::filesystem::path dir = std::filesystem::path(base_path).parent_path();
        std::string basename = std::filesystem::path(base_path).filename().string();
        
        if (std::filesystem::exists(dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.path().filename().string().find(basename) == 0) {
                    count++;
                }
            }
        }
        return count;
    }

    std::string read_log_file(const std::string& filepath) {
        // Retry up to 5 times with 100ms delay between attempts
        for (int i = 0; i < 5; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (std::filesystem::exists(filepath)) {
                std::ifstream file(filepath);
                std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                file.close();
                
                // If content is not empty or this is the last attempt, return it
                if (!content.empty() || i == 4) {
                    return content;
                }
            }
        }
        return "";
    }

    std::string test_log_dir_;
};

TEST_F(LoggerTest, DefaultLogLevel) {
    Logger::instance().set_level(LogLevel::INFO);
    
    std::string log_path = get_test_log_path("default_level.log");
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.enable_stdout = false;
    Logger::instance().set_config(config);
    
    Logger::instance().debug("This should not appear");
    Logger::instance().flush();
    
    std::ifstream file(log_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    EXPECT_EQ(content.find("DEBUG"), std::string::npos);
}

TEST_F(LoggerTest, LogLevelDebug) {
    Logger::instance().set_level(LogLevel::DEBUG);
    
    std::string log_path = get_test_log_path("debug_level.log");
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.enable_stdout = false;
    Logger::instance().set_config(config);
    
    Logger::instance().debug("Debug message");
    Logger::instance().flush();
    
    std::string content = read_log_file(log_path);
    EXPECT_NE(content.find("DEBUG"), std::string::npos);
    EXPECT_NE(content.find("Debug message"), std::string::npos);
}

TEST_F(LoggerTest, LogLevelInfo) {
    Logger::instance().set_level(LogLevel::INFO);

    std::string log_path = get_test_log_path("info_level.log");
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.enable_stdout = false;
    Logger::instance().set_config(config);

    Logger::instance().info("Info message");
    Logger::instance().flush();

    std::string content = read_log_file(log_path);
    EXPECT_NE(content.find("INFO"), std::string::npos);
    EXPECT_NE(content.find("Info message"), std::string::npos);
}

TEST_F(LoggerTest, LogLevelWarning) {
    Logger::instance().set_level(LogLevel::INFO);
    
    std::string log_path = get_test_log_path("warning_level.log");
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.enable_stdout = false;
    Logger::instance().set_config(config);
    
    Logger::instance().warning("Warning message");
    Logger::instance().flush();
    
    std::string content = read_log_file(log_path);
    EXPECT_NE(content.find("WARN"), std::string::npos);
    EXPECT_NE(content.find("Warning message"), std::string::npos);
}

TEST_F(LoggerTest, LogLevelError) {
    Logger::instance().set_level(LogLevel::INFO);
    
    std::string log_path = get_test_log_path("error_level.log");
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.enable_stdout = false;
    Logger::instance().set_config(config);
    
    Logger::instance().error("Error message");
    Logger::instance().flush();
    
    std::string content = read_log_file(log_path);
    EXPECT_NE(content.find("ERROR"), std::string::npos);
    EXPECT_NE(content.find("Error message"), std::string::npos);
}

TEST_F(LoggerTest, LogFormat) {
    Logger::instance().set_level(LogLevel::INFO);
    
    std::string log_path = get_test_log_path("format.log");
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.enable_stdout = false;
    Logger::instance().set_config(config);
    
    Logger::instance().info("Test message");
    Logger::instance().flush();
    
    std::string content = read_log_file(log_path);
    // Check for timestamp format [YYYY-MM-DD HH:MM:SS.mmm]
    EXPECT_TRUE(content.find("[") == 0);
    EXPECT_NE(content.find("]"), std::string::npos);
    EXPECT_NE(content.find("INFO"), std::string::npos);
    EXPECT_NE(content.find("Test message"), std::string::npos);
}

TEST_F(LoggerTest, FileLoggingDisabled) {
    LogConfig config;
    config.enable_file_logging = false;
    config.enable_stdout = false;
    
    Logger::instance().set_config(config);
    
    std::string log_path = get_test_log_path("test.log");
    Logger::instance().info("This should not create a file");
    
    EXPECT_FALSE(std::filesystem::exists(log_path));
}

TEST_F(LoggerTest, FileLoggingEnabled) {
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = get_test_log_path("test.log");
    config.enable_stdout = false;
    
    Logger::instance().set_config(config);
    
    Logger::instance().info("Test log message");
    Logger::instance().flush();
    
    std::string log_path = get_test_log_path("test.log");
    EXPECT_TRUE(std::filesystem::exists(log_path));
    
    std::string content = read_log_file(log_path);
    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("Test log message"), std::string::npos);
}

TEST_F(LoggerTest, LogDirectoryCreation) {
    std::string nested_path = get_test_log_path("nested/dir/test.log");
    
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = nested_path;
    config.enable_stdout = false;
    config.rotation_type = RotationType::NONE;
    
    Logger::instance().set_config(config);
    
    Logger::instance().info("Test message");
    Logger::instance().flush();
    
    EXPECT_TRUE(std::filesystem::exists(nested_path));
    EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(nested_path).parent_path()));
}

TEST_F(LoggerTest, SizeBasedRotation) {
    std::string log_path = get_test_log_path("size_rotation.log");
    
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.rotation_type = RotationType::SIZE;
    config.max_file_size = 1024;  // 1 KB
    config.max_backup_files = 3;
    config.enable_stdout = false;
    
    Logger::instance().set_config(config);
    
    // Write enough logs to trigger rotation
    // Each message is ~100 bytes, so 20 messages should be enough for 1KB
    // Write more to ensure rotation happens
    for (int i = 0; i < 50; i++) {
        Logger::instance().info("This is a long log message to fill up the log file quickly. Message number: " + std::to_string(i) + " with some extra text to ensure we reach the size limit");
    }
    Logger::instance().flush();
    
    // Wait a bit for file operations to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check if rotation occurred
    std::string backup1 = get_backup_path(log_path, 1);
    EXPECT_TRUE(std::filesystem::exists(backup1)) << "Backup file " << backup1 << " was not created";
    
    // Check max backup files limit
    std::string backup4 = get_backup_path(log_path, 4);
    EXPECT_FALSE(std::filesystem::exists(backup4));
}

TEST_F(LoggerTest, MaxBackupFilesLimit) {
    std::string log_path = get_test_log_path("max_backups.log");
    
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.rotation_type = RotationType::SIZE;
    config.max_file_size = 512;  // 512 bytes - very small to force multiple rotations
    config.max_backup_files = 2;
    config.enable_stdout = false;
    
    Logger::instance().set_config(config);
    
    // Write many logs to force multiple rotations
    for (int i = 0; i < 200; i++) {
        Logger::instance().info("Long message " + std::to_string(i) + " to trigger rotation");
    }
    Logger::instance().flush();
    
    // Check that only max_backup_files + 1 (current) files exist
    int file_count = count_backup_files(log_path);
    EXPECT_LE(file_count, config.max_backup_files + 1);
    
    // Check that .3 doesn't exist (should have been deleted)
    std::string backup3 = get_backup_path(log_path, 3);
    EXPECT_FALSE(std::filesystem::exists(backup3));
}

TEST_F(LoggerTest, NoRotation) {
    std::string log_path = get_test_log_path("no_rotation.log");
    
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.rotation_type = RotationType::NONE;
    config.enable_stdout = false;
    
    Logger::instance().set_config(config);
    
    // Write logs
    for (int i = 0; i < 50; i++) {
        Logger::instance().info("Message " + std::to_string(i));
    }
    Logger::instance().flush();
    
    // Check that no backup files were created
    std::string backup1 = get_backup_path(log_path, 1);
    EXPECT_FALSE(std::filesystem::exists(backup1));
    
    // Check that original file exists and contains logs
    EXPECT_TRUE(std::filesystem::exists(log_path));
    EXPECT_GT(get_file_size(log_path), 0);
    
    // Verify content
    std::string content = read_log_file(log_path);
    EXPECT_NE(content.find("Message 0"), std::string::npos);
    EXPECT_NE(content.find("Message 49"), std::string::npos);
}

TEST_F(LoggerTest, CombinedRotation) {
    std::string log_path = get_test_log_path("combined_rotation.log");
    
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.rotation_type = RotationType::BOTH;
    config.max_file_size = 1024;  // 1 KB
    config.rotation_interval = std::chrono::minutes(0);
    config.max_backup_files = 3;
    config.enable_stdout = false;
    
    Logger::instance().set_config(config);
    
    // Write logs to trigger size-based rotation
    for (int i = 0; i < 50; i++) {
        Logger::instance().info("Combined rotation test message " + std::to_string(i) + " with extra text to ensure we reach the size limit for rotation");
    }
    Logger::instance().flush();
    
    // Wait a bit for file operations to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check if size-based rotation occurred
    std::string backup1 = get_backup_path(log_path, 1);
    EXPECT_TRUE(std::filesystem::exists(backup1)) << "Backup file " << backup1 << " was not created";
}

TEST_F(LoggerTest, LogFilePersistence) {
    std::string log_path = get_test_log_path("persistence.log");
    
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.enable_stdout = false;
    config.rotation_type = RotationType::NONE;
    
    Logger::instance().set_config(config);
    
    Logger::instance().info("First message");
    Logger::instance().flush();
    
    size_t size1 = get_file_size(log_path);
    EXPECT_GT(size1, 0);
    
    Logger::instance().info("Second message");
    Logger::instance().flush();
    
    size_t size2 = get_file_size(log_path);
    EXPECT_GT(size2, size1);
    
    // Verify content
    std::ifstream file(log_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    EXPECT_NE(content.find("First message"), std::string::npos);
    EXPECT_NE(content.find("Second message"), std::string::npos);
}

TEST_F(LoggerTest, MultipleLoggers) {
    std::string log_path1 = get_test_log_path("multi1.log");
    std::string log_path2 = get_test_log_path("multi2.log");
    
    // First logger
    LogConfig config1;
    config1.enable_file_logging = true;
    config1.log_file_path = log_path1;
    config1.enable_stdout = false;
    config1.rotation_type = RotationType::NONE;
    Logger::instance().set_config(config1);
    
    Logger::instance().info("Logger 1 message");
    Logger::instance().flush();
    
    // Second logger (same instance, different config)
    LogConfig config2;
    config2.enable_file_logging = true;
    config2.log_file_path = log_path2;
    config2.enable_stdout = false;
    config2.rotation_type = RotationType::NONE;
    Logger::instance().set_config(config2);
    
    Logger::instance().info("Logger 2 message");
    Logger::instance().flush();
    
    // Both files should exist
    EXPECT_TRUE(std::filesystem::exists(log_path1));
    EXPECT_TRUE(std::filesystem::exists(log_path2));
    
    // Verify content
    std::string content1 = read_log_file(log_path1);
    std::string content2 = read_log_file(log_path2);
    
    EXPECT_NE(content1.find("Logger 1 message"), std::string::npos);
    EXPECT_NE(content2.find("Logger 2 message"), std::string::npos);
}

TEST_F(LoggerTest, StdoutLogging) {
    LogConfig config;
    config.enable_file_logging = false;
    config.enable_stdout = true;
    
    Logger::instance().set_config(config);
    Logger::instance().set_level(LogLevel::INFO);
    
    testing::internal::CaptureStdout();
    Logger::instance().info("Stdout test message");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Wait for async logger
    std::string output = testing::internal::GetCapturedStdout();
    
    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("Stdout test message"), std::string::npos);
}

TEST_F(LoggerTest, StdoutDisabled) {
    LogConfig config;
    config.enable_file_logging = false;
    config.enable_stdout = false;
    
    Logger::instance().set_config(config);
    Logger::instance().set_level(LogLevel::INFO);
    
    testing::internal::CaptureStdout();
    Logger::instance().info("This should not appear");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Wait for async logger
    std::string output = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(output.empty());
}

TEST_F(LoggerTest, EmptyLogFile) {
    std::string log_path = get_test_log_path("empty.log");

    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.enable_stdout = false;
    config.rotation_type = RotationType::NONE;

    Logger::instance().set_config(config);

    // File is created immediately when config is set
    // Verify file exists
    EXPECT_TRUE(std::filesystem::exists(log_path));

    // File should be empty initially (or have minimal content)
    size_t initial_size = get_file_size(log_path);
    
    // After writing at least one log, file should exist and have content
    Logger::instance().info("First log");
    Logger::instance().flush();

    // Wait for file operations to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(std::filesystem::exists(log_path));
    EXPECT_GT(get_file_size(log_path), initial_size);

    std::string content = read_log_file(log_path);
    EXPECT_NE(content.find("First log"), std::string::npos);
}

TEST_F(LoggerTest, SpecialCharactersInLog) {
    std::string log_path = get_test_log_path("special.log");
    
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.enable_stdout = false;
    config.rotation_type = RotationType::NONE;
    
    Logger::instance().set_config(config);
    
    Logger::instance().info("Special chars: !@#$%^&*()_+-={}[]|\\:;\"'<>,.?/~`");
    Logger::instance().info("Unicode: 你好世界 🌍");
    Logger::instance().flush();
    
    std::string content = read_log_file(log_path);
    EXPECT_NE(content.find("!@#$%^&*"), std::string::npos);
    EXPECT_NE(content.find("你好世界"), std::string::npos);
}

TEST_F(LoggerTest, LogRotationSequence) {
    std::string log_path = get_test_log_path("sequence.log");
    
    LogConfig config;
    config.enable_file_logging = true;
    config.log_file_path = log_path;
    config.rotation_type = RotationType::SIZE;
    config.max_file_size = 512;  // Small size to force multiple rotations
    config.max_backup_files = 3;
    config.enable_stdout = false;
    
    Logger::instance().set_config(config);
    
    // First rotation - write enough data to exceed 512 bytes
    for (int i = 0; i < 30; i++) {
        Logger::instance().info("First batch " + std::to_string(i) + " with extra text to ensure we reach the size limit for rotation");
    }
    Logger::instance().flush();
    
    // Wait for file operations
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Second rotation - write more data to trigger another rotation
    for (int i = 0; i < 30; i++) {
        Logger::instance().info("Second batch " + std::to_string(i) + " with extra text to ensure we reach the size limit for rotation");
    }
    Logger::instance().flush();
    
    // Wait for file operations
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check that backups are in correct order
    std::string backup1 = get_backup_path(log_path, 1);
    std::string backup2 = get_backup_path(log_path, 2);
    
    EXPECT_TRUE(std::filesystem::exists(backup1)) << "Backup file " << backup1 << " was not created";
    EXPECT_TRUE(std::filesystem::exists(backup2)) << "Backup file " << backup2 << " was not created";
}

TEST_F(LoggerTest, ConfigParsingEnableFileLogging) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-file", "/tmp/test.log",
        "--no-xdg"
    };
    int argc = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_TRUE(config.enable_file_logging);
    EXPECT_EQ(config.log_file_path, "/tmp/test.log");
}

TEST_F(LoggerTest, ConfigParsingLogRotationSize) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-rotation", "size",
        "--log-max-size", "20",
        "--no-xdg"
    };
    int argc = 7;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.log_rotation_type, "size");
    EXPECT_EQ(config.log_max_file_size, 20 * 1024 * 1024);
}

TEST_F(LoggerTest, ConfigParsingLogRotationTime) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-rotation", "time",
        "--log-rotation-interval", "60",
        "--no-xdg"
    };
    int argc = 7;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.log_rotation_type, "time");
    EXPECT_EQ(config.log_rotation_interval_minutes, 60);
}

TEST_F(LoggerTest, ConfigParsingLogRotationBoth) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-rotation", "both",
        "--no-xdg"
    };
    int argc = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.log_rotation_type, "both");
}

TEST_F(LoggerTest, ConfigParsingLogMaxBackups) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-max-backups", "10",
        "--no-xdg"
    };
    int argc = 5;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(config.log_max_backup_files, 10);
}

TEST_F(LoggerTest, ConfigParsingInvalidLogRotationType) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-rotation", "invalid",
        "--no-xdg"
    };
    int argc = 5;

    EXPECT_THROW(
        ConfigParser::parse(argc, const_cast<char**>(argv)),
        std::runtime_error
    );
}

TEST_F(LoggerTest, ConfigParsingInvalidLogMaxSize) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-max-size", "invalid",
        "--no-xdg"
    };
    int argc = 5;

    EXPECT_THROW(
        ConfigParser::parse(argc, const_cast<char**>(argv)),
        std::runtime_error
    );
}

TEST_F(LoggerTest, ConfigParsingInvalidLogRotationInterval) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-rotation-interval", "invalid",
        "--no-xdg"
    };
    int argc = 5;

    EXPECT_THROW(
        ConfigParser::parse(argc, const_cast<char**>(argv)),
        std::runtime_error
    );
}

TEST_F(LoggerTest, ConfigParsingInvalidLogMaxBackups) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-max-backups", "invalid",
        "--no-xdg"
    };
    int argc = 5;

    EXPECT_THROW(
        ConfigParser::parse(argc, const_cast<char**>(argv)),
        std::runtime_error
    );
}

TEST_F(LoggerTest, ConfigValidationLogMaxSizeTooSmall) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-max-size", "0",
        "--no-xdg"
    };
    int argc = 5;

    EXPECT_THROW(
        ConfigParser::parse(argc, const_cast<char**>(argv)),
        std::runtime_error
    );
}

TEST_F(LoggerTest, ConfigValidationLogMaxSizeTooLarge) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-max-size", "2048",
        "--no-xdg"
    };
    int argc = 5;

    EXPECT_THROW(
        ConfigParser::parse(argc, const_cast<char**>(argv)),
        std::runtime_error
    );
}

TEST_F(LoggerTest, ConfigValidationLogRotationIntervalTooSmall) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-rotation-interval", "0",
        "--no-xdg"
    };
    int argc = 5;

    EXPECT_THROW(
        ConfigParser::parse(argc, const_cast<char**>(argv)),
        std::runtime_error
    );
}

TEST_F(LoggerTest, ConfigValidationLogRotationIntervalTooLarge) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-rotation-interval", "20000",
        "--no-xdg"
    };
    int argc = 5;

    EXPECT_THROW(
        ConfigParser::parse(argc, const_cast<char**>(argv)),
        std::runtime_error
    );
}

TEST_F(LoggerTest, ConfigValidationLogMaxBackupsTooLarge) {
    const char* argv[] = {
        "ip_server",
        "--enable-file-logging", "true",
        "--log-max-backups", "200",
        "--no-xdg"
    };
    int argc = 5;

    EXPECT_THROW(
        ConfigParser::parse(argc, const_cast<char**>(argv)),
        std::runtime_error
    );
}

TEST_F(LoggerTest, DefaultLogConfigValues) {
    const char* argv[] = {"ip_server", "--no-xdg"};
    int argc = 2;

    auto config = ConfigParser::parse(argc, const_cast<char**>(argv));

    EXPECT_FALSE(config.enable_file_logging);
    EXPECT_EQ(config.log_file_path, "logs/ip_server.log");
    EXPECT_EQ(config.log_rotation_type, "size");
    EXPECT_EQ(config.log_max_file_size, 10 * 1024 * 1024);
    EXPECT_EQ(config.log_rotation_interval_minutes, 1440);
    EXPECT_EQ(config.log_max_backup_files, 5);
    EXPECT_TRUE(config.log_enable_stdout);
}