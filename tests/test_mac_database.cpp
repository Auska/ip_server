#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <thread>

#include "mac_database.h"
#include "service/mac_lookup_service.h"
#include "test_utils.h"

using namespace ip_server;

class MACDatabaseTest : public ::testing::Test {
   protected:
    void SetUp() override {
        std::filesystem::path const project_root = test::find_project_root();

        oui_db_path = project_root / "db" / "master_oui.db";

        if (!std::filesystem::exists(oui_db_path)) {
            GTEST_SKIP() << "OUI database file not found. Expected at: " << oui_db_path.string()
                         << ". Skipping MAC database tests.";
        }
    }

    std::filesystem::path oui_db_path;
};

TEST_F(MACDatabaseTest, OUIDatabaseOpenSuccess) {
    OUIDatabase db;
    EXPECT_TRUE(db.open(oui_db_path.string()));
    EXPECT_TRUE(db.is_open());
}

TEST_F(MACDatabaseTest, OUIDatabaseOpenFailure) {
    OUIDatabase db;
    EXPECT_FALSE(db.open("/nonexistent/path/to/database.db"));
    EXPECT_FALSE(db.is_open());
}

TEST_F(MACDatabaseTest, OUIDatabaseLookupValidMAC) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("00:1A:2B:3C:4D:5E");

    EXPECT_TRUE(result.contains("found"));
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_TRUE(result.contains("mac"));
    EXPECT_EQ(result["mac"], "00:1A:2B:3C:4D:5E");
    EXPECT_TRUE(result.contains("oui"));
    EXPECT_EQ(result["oui"], "00:1A:2B");
}

TEST_F(MACDatabaseTest, OUIDatabaseLookupWithHyphenSeparator) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("00-1A-2B-3C-4D-5E");

    EXPECT_TRUE(result.contains("found"));
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_TRUE(result.contains("mac"));
    EXPECT_EQ(result["mac"], "00-1A-2B-3C-4D-5E");
    EXPECT_TRUE(result.contains("oui"));
    EXPECT_EQ(result["oui"], "00:1A:2B");
}

TEST_F(MACDatabaseTest, OUIDatabaseLookupWithoutSeparator) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("001A2B3C4D5E");

    EXPECT_TRUE(result.contains("found"));
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_TRUE(result.contains("mac"));
    EXPECT_EQ(result["mac"], "001A2B3C4D5E");
    EXPECT_TRUE(result.contains("oui"));
    EXPECT_EQ(result["oui"], "00:1A:2B");
}

TEST_F(MACDatabaseTest, OUIDatabaseLookupLowercase) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("00:1a:2b:3c:4d:5e");

    EXPECT_TRUE(result.contains("found"));
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_TRUE(result.contains("oui"));
    EXPECT_EQ(result["oui"], "00:1A:2B");
}

TEST_F(MACDatabaseTest, OUIDatabaseLookupMixedCase) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("00:1a:2B:3c:4D:5e");

    EXPECT_TRUE(result.contains("found"));
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_TRUE(result.contains("oui"));
    EXPECT_EQ(result["oui"], "00:1A:2B");
}

TEST_F(MACDatabaseTest, OUIDatabaseLookupInvalidMAC) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("invalid.mac.address");

    EXPECT_TRUE(result.contains("error"));
}

TEST_F(MACDatabaseTest, OUIDatabaseLookupTooShort) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("00:1A:2B");

    EXPECT_TRUE(result.contains("error"));
}

TEST_F(MACDatabaseTest, OUIDatabaseLookupTooLong) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("00:1A:2B:3C:4D:5E:6F");

    EXPECT_TRUE(result.contains("error"));
}

TEST_F(MACDatabaseTest, OUIDatabaseLookupNonExistentOUI) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("FF:FF:FF:FF:FF:FF");

    EXPECT_TRUE(result.contains("found"));
    EXPECT_FALSE(result["found"].get<bool>());
    EXPECT_TRUE(result.contains("mac"));
    EXPECT_EQ(result["mac"], "FF:FF:FF:FF:FF:FF");
    EXPECT_TRUE(result.contains("oui"));
    EXPECT_EQ(result["oui"], "FF:FF:FF");
}

TEST_F(MACDatabaseTest, OUIDatabaseLookupContainsManufacturer) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("00:1A:2B:3C:4D:5E");

    EXPECT_TRUE(result.contains("manufacturer"));
    EXPECT_FALSE(result["manufacturer"].get<std::string>().empty());
}

TEST_F(MACDatabaseTest, OUIDatabaseLookupContainsRegistry) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("00:1A:2B:3C:4D:5E");

    EXPECT_TRUE(result.contains("registry"));
    EXPECT_FALSE(result["registry"].get<std::string>().empty());
}

TEST_F(MACDatabaseTest, OUIDatabaseMoveConstructor) {
    OUIDatabase db1;
    ASSERT_TRUE(db1.open(oui_db_path.string()));

    OUIDatabase db2 = std::move(db1);

    EXPECT_FALSE(db1.is_open());
    EXPECT_TRUE(db2.is_open());

    auto result = db2.lookup("00:1A:2B:3C:4D:5E");
    EXPECT_TRUE(result.contains("found"));
}

TEST_F(MACDatabaseTest, OUIDatabaseMoveAssignment) {
    OUIDatabase db1, db2;
    ASSERT_TRUE(db1.open(oui_db_path.string()));

    db2 = std::move(db1);

    EXPECT_FALSE(db1.is_open());
    EXPECT_TRUE(db2.is_open());
}

TEST_F(MACDatabaseTest, OUIDatabaseClose) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));
    EXPECT_TRUE(db.is_open());

    db.close();
    EXPECT_FALSE(db.is_open());
}

TEST_F(MACDatabaseTest, OUIDatabaseReopen) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));
    EXPECT_TRUE(db.is_open());

    db.close();
    EXPECT_FALSE(db.is_open());

    EXPECT_TRUE(db.open(oui_db_path.string()));
    EXPECT_TRUE(db.is_open());
}

TEST_F(MACDatabaseTest, OUIDatabaseLookupWhenNotOpen) {
    OUIDatabase db;

    auto result = db.lookup("00:1A:2B:3C:4D:5E");

    EXPECT_TRUE(result.contains("error"));
    EXPECT_EQ(result["error"], "Database not open");
}

class MACLookupServiceTest : public ::testing::Test {
   protected:
    void SetUp() override {
        std::filesystem::path const project_root = test::find_project_root();

        oui_db_path = project_root / "db" / "master_oui.db";

        if (!std::filesystem::exists(oui_db_path)) {
            GTEST_SKIP() << "OUI database file not found. Expected at: " << oui_db_path.string()
                         << ". Skipping MAC lookup service tests.";
        }
    }

    std::filesystem::path oui_db_path;
};

TEST_F(MACLookupServiceTest, MACLookupServiceInitialization) {
    EXPECT_NO_THROW({ MACLookupService service(oui_db_path.string(), 1000); });
}

TEST_F(MACLookupServiceTest, MACLookupServiceInitializationFailure) {
    EXPECT_THROW(
        { MACLookupService service("/nonexistent/path/to/database.db", 1000); },
        std::runtime_error);
}

TEST_F(MACLookupServiceTest, MACLookupServiceLookup) {
    MACLookupService service(oui_db_path.string(), 1000);

    auto result = service.lookup("00:1A:2B:3C:4D:5E");

    EXPECT_TRUE(result.data_.contains("found"));
    EXPECT_TRUE(result.data_["found"].get<bool>());
    EXPECT_TRUE(result.data_.contains("mac"));
}

TEST_F(MACLookupServiceTest, MACLookupServiceCacheEnabled) {
    MACLookupService service(oui_db_path.string(), 1000);

    // First lookup
    auto result1 = service.lookup("00:1A:2B:3C:4D:5E");
    EXPECT_FALSE(result1.cache_hit_);

    // Second lookup (should hit cache)
    auto result2 = service.lookup("00:1A:2B:3C:4D:5E");
    EXPECT_TRUE(result2.cache_hit_);
}

TEST_F(MACLookupServiceTest, MACLookupServiceSetCacheSize) {
    MACLookupService service(oui_db_path.string(), 1000);

    service.lookup("00:1A:2B:3C:4D:5E");
}

TEST_F(MACLookupServiceTest, MACLookupServiceIsOUIDBOpen) {
    MACLookupService service(oui_db_path.string(), 1000);

    EXPECT_TRUE(service.is_oui_db_open());
}

TEST_F(MACLookupServiceTest, MACLookupServiceLatencyTracking) {
    MACLookupService service(oui_db_path.string(), 1000);

    auto result = service.lookup("00:1A:2B:3C:4D:5E");

    EXPECT_GE(result.latency_ms_, 0.0);
}

TEST_F(MACLookupServiceTest, MACLookupServiceConcurrentLookups) {
    MACLookupService service(oui_db_path.string(), 1000);

    std::vector<std::future<LookupResult>> futures;

    // Launch concurrent lookups
    for (int i = 0; i < 10; ++i) {
        futures.push_back(std::async(std::launch::async,
                                     [&service]() { return service.lookup("00:1A:2B:3C:4D:5E"); }));
    }

    // Wait for all futures to complete
    for (auto& future : futures) {
        auto result = future.get();
        EXPECT_TRUE(result.data_.contains("found"));
    }
}

TEST_F(MACLookupServiceTest, MACLookupServiceMultipleOUIs) {
    MACLookupService service(oui_db_path.string(), 1000);

    auto result1 = service.lookup("00:1A:2B:3C:4D:5E");
    auto result2 = service.lookup("F4:EA:B5:12:34:56");
    auto result3 = service.lookup("08:EA:44:AB:CD:EF");

    EXPECT_TRUE(result1.data_["found"].get<bool>());
    EXPECT_TRUE(result2.data_["found"].get<bool>());
    EXPECT_TRUE(result3.data_["found"].get<bool>());

    EXPECT_NE(result1.data_["oui"], result2.data_["oui"]);
    EXPECT_NE(result2.data_["oui"], result3.data_["oui"]);
}

// ─── MAC database edge-case tests ─────────────────────────────────

TEST_F(MACDatabaseTest, EmptyMACAddress) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("");
    EXPECT_TRUE(result.contains("error"));
}

TEST_F(MACDatabaseTest, ZeroMACAddress) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("00:00:00:00:00:00");
    EXPECT_TRUE(result.contains("found"));
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_EQ(result["mac"], "00:00:00:00:00:00");
}

TEST_F(MACDatabaseTest, SQLInjectionAttempt) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("'; DROP TABLE oui_registry; --");
    EXPECT_TRUE(result.contains("error"));
}

TEST_F(MACDatabaseTest, VeryLongMACString) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    std::string long_mac(10000, 'A');
    auto result = db.lookup(long_mac);
    EXPECT_TRUE(result.contains("error"));
}

TEST_F(MACDatabaseTest, InvalidHexCharsG) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("GG:GG:GG:GG:GG:GG");
    EXPECT_TRUE(result.contains("error"));
}

TEST_F(MACDatabaseTest, OnlySeparators) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));

    auto result = db.lookup("::::::");
    EXPECT_TRUE(result.contains("error"));
}

TEST_F(MACDatabaseTest, LookupAfterClose) {
    OUIDatabase db;
    ASSERT_TRUE(db.open(oui_db_path.string()));
    db.close();

    auto result = db.lookup("00:1A:2B:3C:4D:5E");
    EXPECT_TRUE(result.contains("error"));
}

TEST_F(MACLookupServiceTest, CacheSizeZero) {
    MACLookupService service(oui_db_path.string(), 0);
    auto result = service.lookup("00:1A:2B:3C:4D:5E");
    EXPECT_TRUE(result.data_.contains("found"));
}

TEST_F(MACLookupServiceTest, GetCacheStatsEmpty) {
    MACLookupService service(oui_db_path.string(), 100);

    auto stats = service.get_cache_stats();
    EXPECT_EQ(stats.total_lookups_, 0);
    EXPECT_EQ(stats.hits_, 0);
    EXPECT_EQ(stats.misses_, 0);
}

TEST_F(MACLookupServiceTest, LookupSameMACRepeatedly) {
    MACLookupService service(oui_db_path.string(), 100);

    // First lookup — cache miss
    auto result1 = service.lookup("00:1A:2B:3C:4D:5E");
    EXPECT_TRUE(result1.data_["found"].get<bool>());
    EXPECT_FALSE(result1.cache_hit_);

    // Second lookup — should be cache hit
    auto result2 = service.lookup("00:1A:2B:3C:4D:5E");
    EXPECT_TRUE(result2.data_["found"].get<bool>());
    EXPECT_TRUE(result2.cache_hit_);
}