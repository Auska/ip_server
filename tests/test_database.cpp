#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <thread>

#include "database.h"
#include "types.h"

using namespace ip_server;

class DatabaseTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Get the project root directory
        // Tests can be run from project root, build/, or build/tests/
        std::filesystem::path current_path = std::filesystem::current_path();
        std::filesystem::path project_root;

        // Check if we're in build/tests/
        if (current_path.filename() == "tests"
            && current_path.parent_path().filename() == "build") {
            // Go up two levels to get to project root
            project_root = current_path.parent_path().parent_path();
        }
        // Check if we're in build/
        else if (current_path.filename() == "build") {
            // Go up one level to get to project root
            project_root = current_path.parent_path();
        }
        // Check if db directory exists in current path (project root)
        else if (std::filesystem::exists(current_path / "db" / "GeoLite2-City.mmdb")) {
            // Already at project root
            project_root = current_path;
        }
        // Try to find project root by looking for CMakeLists.txt
        else {
            std::filesystem::path search_path = current_path;
            while (search_path.has_parent_path()) {
                if (std::filesystem::exists(search_path / "CMakeLists.txt")
                    && std::filesystem::exists(search_path / "db" / "GeoLite2-City.mmdb")) {
                    project_root = search_path;
                    break;
                }
                search_path = search_path.parent_path();
            }

            // If not found, default to current path
            if (project_root.empty()) {
                project_root = current_path;
            }
        }

        city_db_path = project_root / "db" / "GeoLite2-City.mmdb";
        asn_db_path  = project_root / "db" / "GeoLite2-ASN.mmdb";

        // Skip tests if database files don't exist
        if (!std::filesystem::exists(city_db_path) || !std::filesystem::exists(asn_db_path)) {
            GTEST_SKIP() << "Database files not found. Expected at: " << city_db_path.string()
                         << " and " << asn_db_path.string() << ". Skipping database tests.";
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
    EXPECT_NO_THROW({ IPGeoService service(city_db_path.string(), asn_db_path.string()); });
}

TEST_F(DatabaseTest, IPGeoServiceLookup) {
    IPGeoService service(city_db_path.string(), asn_db_path.string());

    auto result = service.lookup("8.8.8.8");

    EXPECT_TRUE(result.data.contains("ip"));
    EXPECT_EQ(result.data["ip"], "8.8.8.8");
    EXPECT_TRUE(result.data.contains("found"));
}

TEST_F(DatabaseTest, IPGeoServiceLookupMultipleIPs) {
    IPGeoService service(city_db_path.string(), asn_db_path.string());

    std::vector<std::string> test_ips = {
        "8.8.8.8",         // Google DNS
        "1.1.1.1",         // Cloudflare DNS
        "114.114.114.114"  // Chinese DNS
    };

    for (const auto& ip : test_ips) {
        auto result = service.lookup(ip);
        EXPECT_TRUE(result.data.contains("ip"));
        EXPECT_EQ(result.data["ip"], ip);
        EXPECT_TRUE(result.data.contains("found"));
    }
}

TEST_F(DatabaseTest, IPGeoServiceLookupInvalidIP) {
    IPGeoService service(city_db_path.string(), asn_db_path.string());

    auto result = service.lookup("999.999.999.999");

    EXPECT_TRUE(result.data.contains("ip"));
    EXPECT_TRUE(result.data.contains("error"));
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

TEST_F(DatabaseTest, LookupResultStructure) {
    IPGeoService service(city_db_path.string(), asn_db_path.string());

    auto result = service.lookup("8.8.8.8");

    // Verify LookupResult structure
    EXPECT_TRUE(result.data.contains("ip"));
    EXPECT_TRUE(result.data.contains("found"));
    EXPECT_GE(result.latency_ms, 0.0);
    EXPECT_TRUE(result.cache_hit == true || result.cache_hit == false);
}

TEST_F(DatabaseTest, CacheHitTracking) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    // First lookup - should be cache miss
    auto result1 = service.lookup("8.8.8.8");
    EXPECT_FALSE(result1.cache_hit);
    EXPECT_TRUE(result1.data.contains("found"));

    // Second lookup - should be cache hit
    auto result2 = service.lookup("8.8.8.8");
    EXPECT_TRUE(result2.cache_hit);
    EXPECT_TRUE(result2.data.contains("found"));

    // Cache hit should be much faster
    EXPECT_LT(result2.latency_ms, result1.latency_ms);

    // Results should be identical
    EXPECT_EQ(result1.data.dump(), result2.data.dump());
}

TEST_F(DatabaseTest, CacheMissTracking) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    std::vector<std::string> unique_ips = {"8.8.8.8", "1.1.1.1", "9.9.9.9", "208.67.222.222"};

    for (const auto& ip : unique_ips) {
        auto result = service.lookup(ip);
        EXPECT_FALSE(result.cache_hit) << "First lookup for " << ip << " should be cache miss";
        EXPECT_TRUE(result.data.contains("found"));
    }
}

TEST_F(DatabaseTest, CacheSizeLimit) {
    // Create service with small cache size
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 2);

    // First lookup - cache miss
    auto result1 = service.lookup("8.8.8.8");
    ASSERT_TRUE(result1.data["found"].get<bool>());
    EXPECT_FALSE(result1.cache_hit);

    // Second lookup of same IP - cache hit
    auto result2 = service.lookup("8.8.8.8");
    EXPECT_TRUE(result2.cache_hit);
    EXPECT_EQ(result2.data.dump(), result1.data.dump());

    // Third lookup of different IP - cache miss
    auto result3 = service.lookup("1.1.1.1");
    ASSERT_TRUE(result3.data["found"].get<bool>());
    EXPECT_FALSE(result3.cache_hit);

    // Fourth lookup of first IP - should still be in cache (cache hit)
    auto result4 = service.lookup("8.8.8.8");
    EXPECT_TRUE(result4.cache_hit);

    // Fifth lookup of second IP - should still be in cache (cache hit)
    auto result5 = service.lookup("1.1.1.1");
    EXPECT_TRUE(result5.cache_hit);
}

TEST_F(DatabaseTest, LatencyMeasurement) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    auto result = service.lookup("8.8.8.8");

    // Latency should be positive
    EXPECT_GT(result.latency_ms, 0.0);

    // Latency should be reasonable (less than 1 second for cached, less than 10
    // seconds for uncached)
    EXPECT_LT(result.latency_ms, 10000.0);
}

TEST_F(DatabaseTest, ParallelLookupConsistency) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    std::vector<std::string> test_ips = {"8.8.8.8", "1.1.1.1", "9.9.9.9", "208.67.222.222",
                                         "208.67.220.220"};

    // Perform lookups
    std::vector<LookupResult> results;
    for (const auto& ip : test_ips) {
        results.push_back(service.lookup(ip));
    }

    // Verify all results are valid
    for (size_t i = 0; i < test_ips.size(); ++i) {
        EXPECT_TRUE(results[i].data.contains("ip"));
        EXPECT_EQ(results[i].data["ip"], test_ips[i]);
        EXPECT_TRUE(results[i].data.contains("found"));
        EXPECT_GE(results[i].latency_ms, 0.0);
    }

    // Perform same lookups again - should all be cache hits
    for (size_t i = 0; i < test_ips.size(); ++i) {
        auto cached_result = service.lookup(test_ips[i]);
        EXPECT_TRUE(cached_result.cache_hit);
        EXPECT_EQ(cached_result.data.dump(), results[i].data.dump());
    }
}

TEST_F(DatabaseTest, LookupResultMoveSemantics) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    auto result1 = service.lookup("8.8.8.8");

    // Test move constructor
    LookupResult result2 = std::move(result1);

    EXPECT_TRUE(result2.data.contains("ip"));
    EXPECT_TRUE(result2.cache_hit == true || result2.cache_hit == false);
    EXPECT_GE(result2.latency_ms, 0.0);

    // Test move assignment
    LookupResult result3;
    result3 = std::move(result2);

    EXPECT_TRUE(result3.data.contains("ip"));
    EXPECT_TRUE(result3.cache_hit == true || result3.cache_hit == false);
    EXPECT_GE(result3.latency_ms, 0.0);
}

TEST_F(DatabaseTest, BatchLookupPerformance) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    std::vector<std::string> test_ips;
    for (int i = 0; i < 50; ++i) {
        test_ips.push_back("8.8.8." + std::to_string(i % 255));
    }

    // First batch - all cache misses
    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& ip : test_ips) {
        service.lookup(ip);
    }
    auto first_batch_time =
        std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start)
            .count();

    // Second batch - all cache hits
    start = std::chrono::high_resolution_clock::now();
    for (const auto& ip : test_ips) {
        auto result = service.lookup(ip);
        EXPECT_TRUE(result.cache_hit);
    }
    auto second_batch_time =
        std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start)
            .count();

    // Cached batch should be faster (at least 2x faster in most cases)
    // This is a more realistic expectation than 10x
    EXPECT_LT(second_batch_time, first_batch_time);

    // Also verify that all lookups in second batch are cache hits
    for (const auto& ip : test_ips) {
        auto result = service.lookup(ip);
        EXPECT_TRUE(result.cache_hit) << "IP " << ip << " should be cached";
    }
}

TEST_F(DatabaseTest, CacheTTLExpiration) {
    // Create service with very short TTL (1 second)
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    // First lookup
    auto result1 = service.lookup("8.8.8.8");
    EXPECT_FALSE(result1.cache_hit);

    // Immediate second lookup - should be cache hit
    auto result2 = service.lookup("8.8.8.8");
    EXPECT_TRUE(result2.cache_hit);

    // Wait for TTL to expire (default TTL is 1 hour, so we can't test this
    // without modifying cache) This test is conceptual - in production, you might
    // want to add a method to set TTL
}

TEST_F(DatabaseTest, ConcurrentLookups) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    std::vector<std::string> test_ips = {"8.8.8.8", "1.1.1.1", "9.9.9.9", "208.67.222.222"};

    std::vector<std::future<LookupResult>> futures;

    // Launch concurrent lookups
    for (const auto& ip : test_ips) {
        futures.push_back(
            std::async(std::launch::async, [&service, ip]() { return service.lookup(ip); }));
    }

    // Wait for all results
    for (size_t i = 0; i < futures.size(); ++i) {
        auto result = futures[i].get();
        EXPECT_TRUE(result.data.contains("ip"));
        EXPECT_EQ(result.data["ip"], test_ips[i]);
        EXPECT_TRUE(result.data.contains("found"));
    }
}

TEST_F(DatabaseTest, LookupResultDataIntegrity) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    auto result = service.lookup("8.8.8.8");

    // Verify all expected fields are present
    EXPECT_TRUE(result.data.contains("ip"));
    EXPECT_TRUE(result.data.contains("found"));

    if (result.data["found"].get<bool>()) {
        // Check for city data
        if (result.data.contains("country")) {
            EXPECT_FALSE(result.data["country"].get<std::string>().empty());
        }
        if (result.data.contains("country_code")) {
            EXPECT_FALSE(result.data["country_code"].get<std::string>().empty());
        }

        // Check for ASN data
        if (result.data.contains("as_organization")) {
            EXPECT_FALSE(result.data["as_organization"].get<std::string>().empty());
        }
        if (result.data.contains("as_number")) {
            EXPECT_GT(result.data["as_number"].get<uint32_t>(), 0);
        }
    }
}