#include <gtest/gtest.h>
#include "database.h"
#include "types.h"
#include <filesystem>

using namespace ip_server;

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get the project root directory
        std::filesystem::path project_root = std::filesystem::current_path().parent_path();

        city_db_path = project_root / "db" / "GeoLite2-City.mmdb";
        asn_db_path = project_root / "db" / "GeoLite2-ASN.mmdb";

        // Skip tests if database files don't exist
        if (!std::filesystem::exists(city_db_path) || !std::filesystem::exists(asn_db_path)) {
            GTEST_SKIP() << "Database files not found. Skipping database tests.";
        }
    }

    std::filesystem::path city_db_path;
    std::filesystem::path asn_db_path;
};

TEST_F(DatabaseTest, CityDatabaseOpenSuccess) {
    CityDatabase db;
    EXPECT_TRUE(db.open(city_db_path.string()));
    EXPECT_TRUE(db.is_open());
}

TEST_F(DatabaseTest, CityDatabaseOpenFailure) {
    CityDatabase db;
    EXPECT_FALSE(db.open("/nonexistent/path/to/database.mmdb"));
    EXPECT_FALSE(db.is_open());
}

TEST_F(DatabaseTest, CityDatabaseLookupValidIP) {
    CityDatabase db;
    ASSERT_TRUE(db.open(city_db_path.string()));

    auto result = db.lookup("8.8.8.8");

    EXPECT_TRUE(result.contains("found"));
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_TRUE(result.contains("ip"));
    EXPECT_EQ(result["ip"], "8.8.8.8");
}

TEST_F(DatabaseTest, CityDatabaseLookupInvalidIP) {
    CityDatabase db;
    ASSERT_TRUE(db.open(city_db_path.string()));

    auto result = db.lookup("invalid.ip.address");

    EXPECT_TRUE(result.contains("error"));
}

TEST_F(DatabaseTest, CityDatabaseMoveConstructor) {
    CityDatabase db1;
    ASSERT_TRUE(db1.open(city_db_path.string()));

    CityDatabase db2 = std::move(db1);

    EXPECT_FALSE(db1.is_open());
    EXPECT_TRUE(db2.is_open());

    auto result = db2.lookup("8.8.8.8");
    EXPECT_TRUE(result.contains("found"));
}

TEST_F(DatabaseTest, CityDatabaseMoveAssignment) {
    CityDatabase db1, db2;
    ASSERT_TRUE(db1.open(city_db_path.string()));

    db2 = std::move(db1);

    EXPECT_FALSE(db1.is_open());
    EXPECT_TRUE(db2.is_open());
}

TEST_F(DatabaseTest, CityDatabaseClose) {
    CityDatabase db;
    ASSERT_TRUE(db.open(city_db_path.string()));
    EXPECT_TRUE(db.is_open());

    db.close();
    EXPECT_FALSE(db.is_open());
}

TEST_F(DatabaseTest, ASNDatabaseOpenSuccess) {
    ASNDatabase db;
    EXPECT_TRUE(db.open(asn_db_path.string()));
    EXPECT_TRUE(db.is_open());
}

TEST_F(DatabaseTest, ASNDatabaseLookupValidIP) {
    ASNDatabase db;
    ASSERT_TRUE(db.open(asn_db_path.string()));

    auto result = db.lookup("8.8.8.8");

    EXPECT_TRUE(result.contains("found"));
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_TRUE(result.contains("ip"));
    EXPECT_EQ(result["ip"], "8.8.8.8");

    // Google DNS should have AS information
    if (result.contains("as_organization")) {
        EXPECT_FALSE(result["as_organization"].get<std::string>().empty());
    }
}

TEST_F(DatabaseTest, IPGeoServiceInitialization) {
    EXPECT_NO_THROW({
        IPGeoService service(city_db_path.string(), asn_db_path.string());
    });
}

TEST_F(DatabaseTest, IPGeoServiceLookup) {
    IPGeoService service(city_db_path.string(), asn_db_path.string());

    auto result = service.lookup("8.8.8.8");

    EXPECT_TRUE(result.contains("ip"));
    EXPECT_EQ(result["ip"], "8.8.8.8");
    EXPECT_TRUE(result.contains("found"));
}

TEST_F(DatabaseTest, IPGeoServiceLookupMultipleIPs) {
    IPGeoService service(city_db_path.string(), asn_db_path.string());

    std::vector<std::string> test_ips = {
        "8.8.8.8",      // Google DNS
        "1.1.1.1",      // Cloudflare DNS
        "114.114.114.114" // Chinese DNS
    };

    for (const auto& ip : test_ips) {
        auto result = service.lookup(ip);
        EXPECT_TRUE(result.contains("ip"));
        EXPECT_EQ(result["ip"], ip);
        EXPECT_TRUE(result.contains("found"));
    }
}

TEST_F(DatabaseTest, IPGeoServiceLookupInvalidIP) {
    IPGeoService service(city_db_path.string(), asn_db_path.string());

    auto result = service.lookup("999.999.999.999");

    EXPECT_TRUE(result.contains("ip"));
    EXPECT_TRUE(result.contains("error"));
}

TEST_F(DatabaseTest, IPGeoServiceNotCopyable) {
    IPGeoService service(city_db_path.string(), asn_db_path.string());

    // Test that copy constructor is deleted
    EXPECT_FALSE(std::is_copy_constructible<IPGeoService>::value);

    // Test that copy assignment is deleted
    EXPECT_FALSE(std::is_copy_assignable<IPGeoService>::value);
}

TEST_F(DatabaseTest, MaxMindDatabaseNotCopyable) {
    MaxMindDatabase db;

    // Test that copy constructor is deleted
    EXPECT_FALSE(std::is_copy_constructible<MaxMindDatabase>::value);

    // Test that copy assignment is deleted
    EXPECT_FALSE(std::is_copy_assignable<MaxMindDatabase>::value);
}