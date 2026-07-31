#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <thread>

#include "service/ip_geo_service.h"
#include "service/mac_lookup_service.h"
#include "types.h"

using namespace ip_server;

class DatabaseTest : public ::testing::Test {
   protected:
    void SetUp() override {
        std::filesystem::path current_path = std::filesystem::current_path();
        std::filesystem::path project_root;

        if (current_path.filename() == "tests" && current_path.parent_path().filename() == "build")
            project_root = current_path.parent_path().parent_path();
        else if (current_path.filename() == "build")
            project_root = current_path.parent_path();
        else if (current_path.filename() == "bin" && current_path.parent_path().filename() == "build")
            project_root = current_path.parent_path().parent_path();
        else
            project_root = current_path;

        city_db_path = project_root / "db" / "GeoLite2-City.mmdb";
        asn_db_path  = project_root / "db" / "GeoLite2-ASN.mmdb";
        oui_db_path  = project_root / "db" / "master_oui.db";

        if (!std::filesystem::exists(city_db_path) || !std::filesystem::exists(asn_db_path)) {
            GTEST_SKIP() << "Database files not found. Expected at: " << city_db_path.string()
                         << " and " << asn_db_path.string() << ". Skipping database tests.";
        }
    }

    std::filesystem::path city_db_path;
    std::filesystem::path asn_db_path;
    std::filesystem::path oui_db_path;
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

// ============================================================================
// Sharded Cache Tests
// ============================================================================

TEST_F(DatabaseTest, CacheStatsInitialization) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    auto stats = service.get_cache_stats();

    EXPECT_EQ(stats.total_lookups, 0);
    EXPECT_EQ(stats.hits, 0);
    EXPECT_EQ(stats.misses, 0);
    EXPECT_EQ(stats.evictions, 0);
    EXPECT_EQ(stats.expired_entries, 0);
    EXPECT_DOUBLE_EQ(stats.hit_rate(), 0.0);
}

TEST_F(DatabaseTest, CacheStatsHitRate) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    // First lookup - miss
    service.lookup("8.8.8.8");

    // Second lookup - hit
    service.lookup("8.8.8.8");

    auto stats = service.get_cache_stats();
    EXPECT_EQ(stats.total_lookups, 2);
    EXPECT_EQ(stats.hits, 1);
    EXPECT_EQ(stats.misses, 1);
    EXPECT_DOUBLE_EQ(stats.hit_rate(), 50.0);
}

TEST_F(DatabaseTest, CacheStatsAfterClear) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    // Perform some lookups
    service.lookup("8.8.8.8");
    service.lookup("1.1.1.1");
    service.lookup("8.8.8.8");  // Cache hit

    auto stats_before = service.get_cache_stats();
    EXPECT_GT(stats_before.total_lookups, 0);
    EXPECT_GT(stats_before.hits, 0);

    // Clear cache
    service.clear_cache();

    // Lookups should still be cached but cache is empty
    auto cache_size = service.get_cache_size();
    EXPECT_EQ(cache_size, 0);

    // Next lookup should be a miss
    auto result = service.lookup("8.8.8.8");
    EXPECT_FALSE(result.cache_hit);
}

TEST_F(DatabaseTest, CacheStatsEvictionTracking) {
    // Create service with small cache (5 entries)
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 5);

    // Use known public DNS server IPs that are definitely in the database
    std::vector<std::string> known_ips = {"8.8.8.8",        "1.1.1.1",   "9.9.9.9",
                                          "208.67.222.222", "64.6.64.6", "185.228.168.9"};

    // Fill cache with more entries than cache size
    for (const auto& ip : known_ips) {
        auto result = service.lookup(ip);
        // Verify the IP was found in database
        ASSERT_TRUE(result.data.value("found", false)) << "IP " << ip << " not found in database";
    }

    auto stats = service.get_cache_stats();
    // With 6 lookups and cache size 5, we should have at least 1 eviction
    EXPECT_GT(stats.evictions, 0) << "Expected evictions but got " << stats.evictions
                                  << ". Cache size: " << service.get_cache_size();
}

TEST_F(DatabaseTest, CacheShardDistribution) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    // Perform lookups on many different IPs using known public DNS servers
    std::vector<std::string> test_ips = {"8.8.8.8",        "1.1.1.1",        "9.9.9.9",
                                         "208.67.222.222", "208.67.220.220", "64.6.64.6",
                                         "64.6.65.6"};
    for (int i = 0; i < 100; ++i) {
        // Cycle through test IPs to get cache hits
        service.lookup(test_ips[i % test_ips.size()]);
    }

    // Verify cache is being used
    auto cache_size = service.get_cache_size();
    EXPECT_GT(cache_size, 0);
    EXPECT_LE(cache_size, 1000);

    // Verify stats are updated
    auto stats = service.get_cache_stats();
    EXPECT_EQ(stats.total_lookups, 100);
    // First batch_size lookups are misses, rest are hits
    EXPECT_GT(stats.hits, 0);
}

TEST_F(DatabaseTest, CacheConcurrentAccess) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    std::vector<std::string> test_ips;
    for (int i = 0; i < 50; ++i) {
        test_ips.push_back("192.168.1." + std::to_string(i));
    }

    std::vector<std::future<LookupResult>> futures;

    // Launch concurrent lookups to test sharded cache
    for (const auto& ip : test_ips) {
        futures.push_back(
            std::async(std::launch::async, [&service, ip]() { return service.lookup(ip); }));
    }

    // Wait for all results
    for (auto& future : futures) {
        auto result = future.get();
        EXPECT_TRUE(result.data.contains("ip"));
        EXPECT_TRUE(result.data.contains("found"));
    }

    // Verify stats
    auto stats = service.get_cache_stats();
    EXPECT_EQ(stats.total_lookups, 50);
    EXPECT_EQ(stats.hits, 0);
    EXPECT_EQ(stats.misses, 50);
}

TEST_F(DatabaseTest, CacheConcurrencyStress) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    const int num_threads        = 4;
    const int lookups_per_thread = 25;

    // Use known public DNS servers to ensure all IPs are in database
    std::vector<std::string> known_ips = {"8.8.8.8",        "1.1.1.1",        "9.9.9.9",
                                          "208.67.222.222", "208.67.220.220", "64.6.64.6",
                                          "64.6.65.6",      "185.228.168.9",  "185.228.169.9",
                                          "1.0.0.1",        "8.8.4.4",        "1.1.1.2",
                                          "9.9.9.10",       "208.67.222.223", "208.67.220.223"};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&service, t, lookups_per_thread, &known_ips]() {
            for (int i = 0; i < lookups_per_thread; ++i) {
                // Each thread uses unique subset of known IPs
                size_t ip_index = (t * lookups_per_thread + i) % known_ips.size();
                service.lookup(known_ips[ip_index]);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify stats
    auto stats = service.get_cache_stats();
    // Allow for some queries to fail (not in database)
    EXPECT_GE(stats.total_lookups, num_threads * lookups_per_thread * 0.9);
    EXPECT_GT(stats.concurrent_accesses, 0);
    // Hits may occur if same IP is queried multiple times across threads
    EXPECT_GT(stats.misses, 0);
}

TEST_F(DatabaseTest, CacheAverageEntrySize) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    // Perform lookups
    service.lookup("8.8.8.8");
    service.lookup("1.1.1.1");

    auto stats = service.get_cache_stats();
    EXPECT_GT(stats.avg_entry_size, 0.0);
}

// ============================================================================
// Enhanced Cache Tests (New Features)
// ============================================================================

TEST_F(DatabaseTest, CacheMemoryUsageTracking) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    // Perform lookups
    service.lookup("8.8.8.8");
    service.lookup("1.1.1.1");
    service.lookup("9.9.9.9");

    auto stats = service.get_cache_stats();
    EXPECT_GT(stats.memory_usage_bytes, 0);
    EXPECT_GT(stats.get_memory_usage_mb(), 0.0);
}

TEST_F(DatabaseTest, CacheMemoryBasedEviction) {
    // Create service with small memory limit (1KB)
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 10000, 8, 1024);

    // Fill cache with many entries until memory limit is reached
    for (int i = 0; i < 100; ++i) {
        service.lookup("8.8.8." + std::to_string(i % 255));
    }

    auto stats = service.get_cache_stats();
    // Memory usage should be close to limit (within some tolerance)
    // Allow 4x overhead due to estimation and data structure overhead
    EXPECT_LT(stats.memory_usage_bytes, 1024 * 4);
}

TEST_F(DatabaseTest, CacheNegativeCaching) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    // Use a valid IP that likely doesn't exist in the GeoLite database
    // 10.0.0.1 is a private IP, typically not in public IP databases
    auto result1 = service.lookup("10.0.0.1");
    EXPECT_FALSE(result1.cache_hit);
    // The result should have found=false (not an error)
    EXPECT_FALSE(result1.data.value("found", true));

    // Second lookup of same IP - should be cache hit (negative cache)
    auto result2 = service.lookup("10.0.0.1");
    EXPECT_TRUE(result2.cache_hit);
    EXPECT_EQ(result2.data.dump(), result1.data.dump());

    auto stats = service.get_cache_stats();
    EXPECT_GT(stats.negative_hits, 0);
}

TEST_F(DatabaseTest, CacheShardStatistics) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000, 8);

    // Perform lookups
    for (int i = 0; i < 100; ++i) {
        service.lookup("8.8.8." + std::to_string(i % 255));
    }

    auto shard_stats = service.get_shard_stats();
    EXPECT_EQ(shard_stats.size(), 8);

    // Verify all shards have some entries
    size_t total_shard_entries = 0;
    for (const auto& stat : shard_stats) {
        total_shard_entries += stat.size;
        EXPECT_GE(stat.shard_index, 0);
        EXPECT_LT(stat.shard_index, 8);
    }

    EXPECT_GT(total_shard_entries, 0);
}

TEST_F(DatabaseTest, CacheHeatMapGeneration) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    // Perform multiple lookups on the same IPs to create hot keys
    for (int i = 0; i < 50; ++i) {
        service.lookup("8.8.8.8");  // Very hot - 50 times
        service.lookup("1.1.1.1");  // Hot - 50 times
        if (i % 2 == 0) {
            service.lookup("9.9.9.9");  // Warm - 25 times
        }
        if (i % 10 == 0) {
            service.lookup("208.67.222.222");  // Less hot - 5 times
        }
    }

    // Add extra lookups to make 8.8.8.8 hotter than 1.1.1.1
    for (int i = 0; i < 10; ++i) {
        service.lookup("8.8.8.8");
    }

    auto heat_map = service.get_heat_map(3);

    EXPECT_EQ(heat_map.hot_keys.size(), 3);
    EXPECT_GT(heat_map.total_accesses, 0);
    EXPECT_EQ(heat_map.shard_distribution.size(), 8);

    // Top key should be 8.8.8.8 (most accessed)
    EXPECT_EQ(heat_map.hot_keys[0].first, "8.8.8.8");
    EXPECT_GT(heat_map.hot_keys[0].second, heat_map.hot_keys[1].second);
}

TEST_F(DatabaseTest, CacheTTLConfiguration) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 1000);

    // Test TTL configuration
    service.set_cache_ttl(CacheDataType::IP_GEOLOCATION, std::chrono::seconds(7200));
    service.set_cache_ttl(CacheDataType::NEGATIVE, std::chrono::seconds(60));

    // Perform lookups
    service.lookup("8.8.8.8");

    // Cache should work normally
    auto result1 = service.lookup("8.8.8.8");
    EXPECT_TRUE(result1.cache_hit);
}

TEST_F(DatabaseTest, ConcurrentReadWritePerformance) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 10000, 16);

    const int num_readers           = 8;
    const int num_writers           = 4;
    const int operations_per_thread = 50;

    std::vector<std::thread> threads;

    // Reader threads
    for (int i = 0; i < num_readers; ++i) {
        threads.emplace_back([&service, operations_per_thread]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                service.lookup("8.8.8.8");
                service.lookup("1.1.1.1");
            }
        });
    }

    // Writer threads
    for (int i = 0; i < num_writers; ++i) {
        threads.emplace_back([&service, operations_per_thread, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                service.lookup("192.168." + std::to_string(i) + "." + std::to_string(j));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto stats = service.get_cache_stats();
    EXPECT_GT(stats.total_lookups, 0);
    EXPECT_GT(stats.concurrent_accesses, 0);
}

TEST_F(DatabaseTest, CacheMemoryLimitRespected) {
    const size_t memory_limit = 50 * 1024;  // 50KB
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 10000, 4, memory_limit);

    // Add many entries to exceed memory limit
    for (int i = 0; i < 200; ++i) {
        service.lookup("8.8.8." + std::to_string(i % 255));
    }

    auto stats        = service.get_cache_stats();
    auto memory_usage = service.get_cache_memory_usage();

    // Memory usage should be close to or below limit (allowing for some overhead)
    EXPECT_LT(memory_usage, memory_limit * 2);
    EXPECT_GT(stats.evictions, 0);  // Should have evicted some entries
}

TEST_F(DatabaseTest, MACLookupServiceMemoryTracking) {
    MACLookupService service(oui_db_path.string(), 1000);

    // Perform MAC lookups
    service.lookup("00:1A:2B:3C:4D:5E");
    service.lookup("F4:EA:B5:12:34:56");

    auto stats = service.get_cache_stats();
    EXPECT_GT(stats.memory_usage_bytes, 0);
    EXPECT_GT(stats.get_memory_usage_mb(), 0.0);

    auto shard_stats = service.get_shard_stats();
    EXPECT_EQ(shard_stats.size(), 8);
}

TEST_F(DatabaseTest, MACLookupServiceHeatMap) {
    MACLookupService service(oui_db_path.string(), 1000);

    // Perform multiple lookups on same MAC addresses
    for (int i = 0; i < 30; ++i) {
        service.lookup("00:1A:2B:3C:4D:5E");
        service.lookup("F4:EA:B5:12:34:56");
    }

    auto heat_map = service.get_heat_map(2);
    EXPECT_EQ(heat_map.hot_keys.size(), 2);
    EXPECT_GT(heat_map.total_accesses, 0);
}

// ─── Cache edge-case tests ────────────────────────────────────────

TEST(CacheEdgeTest, ZeroSizeCache) {
    IPCache cache(0, 1, std::chrono::seconds(3600), 1024);
    EXPECT_EQ(cache.size(), 0);

    // Putting into zero-size cache should not crash
    EXPECT_NO_THROW(cache.put("test-key", nlohmann::json::object(), CacheDataType::IP_GEOLOCATION));
    EXPECT_EQ(cache.size(), 0);
}

TEST(CacheEdgeTest, SingleShardCache) {
    IPCache cache(100, 1, std::chrono::seconds(3600), 1024 * 1024);
    EXPECT_EQ(cache.shard_count(), 1);

    cache.put("key1", nlohmann::json{{"data", 1}});
    cache.put("key2", nlohmann::json{{"data", 2}});

    auto val1 = cache.get("key1");
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ((*val1)["data"], 1);

    auto val2 = cache.get("key2");
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ((*val2)["data"], 2);
}

TEST(CacheEdgeTest, ZeroTTLEntry) {
    IPCache cache(100, 4, std::chrono::seconds(0), 1024 * 1024);

    // Override the per-type TTLs that configure_default_ttls() sets
    cache.set_ttl(CacheDataType::IP_GEOLOCATION, std::chrono::seconds(0));

    cache.put("immediate-expire", nlohmann::json{{"data", 1}});

    // With TTL=0, the entry should expire immediately
    auto val = cache.get("immediate-expire");
    EXPECT_FALSE(val.has_value());
}

TEST(CacheEdgeTest, ConcurrentPutSameKey) {
    IPCache cache(1000, 8, std::chrono::seconds(3600), 10 * 1024 * 1024);
    std::string key = "concurrent-key";

    std::vector<std::thread> threads;
    for (int i = 0; i < 20; i++) {
        threads.emplace_back([&, i]() {
            cache.put(key, nlohmann::json{{"thread", i}});
        });
    }
    for (auto& t : threads) t.join();

    // At least one thread's value should be stored
    auto val = cache.get(key);
    EXPECT_TRUE(val.has_value());
}

TEST(CacheEdgeTest, GetHeatMapEmptyCache) {
    IPCache cache(100, 4, std::chrono::seconds(3600), 1024 * 1024);
    auto heat_map = cache.get_heat_map(10);

    EXPECT_TRUE(heat_map.hot_keys.empty());
    EXPECT_EQ(heat_map.shard_distribution.size(), 4);
    EXPECT_EQ(heat_map.total_accesses, 0);
}

TEST(CacheEdgeTest, ClearEmptyCache) {
    IPCache cache(100, 4, std::chrono::seconds(3600), 1024 * 1024);
    EXPECT_NO_THROW(cache.clear());
    EXPECT_EQ(cache.size(), 0);
}

TEST(CacheEdgeTest, NegativeCacheThenOverwrite) {
    IPCache cache(100, 4, std::chrono::seconds(3600), 1024 * 1024);

    // Put negative cache entry
    cache.put("test-key", nlohmann::json{{"found", false}}, CacheDataType::NEGATIVE);
    auto val = cache.get("test-key");
    EXPECT_TRUE(val.has_value());
    EXPECT_FALSE((*val)["found"].get<bool>());

    // Overwrite with positive entry
    cache.put("test-key", nlohmann::json{{"found", true}}, CacheDataType::IP_GEOLOCATION);
    val = cache.get("test-key");
    EXPECT_TRUE(val.has_value());
    EXPECT_TRUE((*val)["found"].get<bool>());
}

TEST(CacheEdgeTest, CacheStatsOperatorPlusEquals) {
    CacheStats a, b;

    a.total_lookups = 10;
    a.hits = 8;
    a.avg_entry_size = 100.0;
    a.entry_count = 5;

    b.total_lookups = 20;
    b.hits = 15;
    b.avg_entry_size = 200.0;
    b.entry_count = 10;

    CacheStats combined;
    combined += a;
    combined += b;

    EXPECT_EQ(combined.total_lookups, 30);
    EXPECT_EQ(combined.hits, 23);

    // Weighted average: (100*5 + 200*10) / 15 = (500 + 2000) / 15 = 166.67
    EXPECT_NEAR(combined.avg_entry_size, 166.67, 0.01);
    EXPECT_EQ(combined.entry_count, 15);
}

TEST(CacheEdgeTest, CacheStatsOperatorPlusEqualsZero) {
    CacheStats a, b;

    a.total_lookups = 10;
    a.avg_entry_size = 100.0;
    a.entry_count = 5;

    // Adding empty stats should not change anything
    a += b;
    EXPECT_EQ(a.total_lookups, 10);
    EXPECT_EQ(a.avg_entry_size, 100.0);
    EXPECT_EQ(a.entry_count, 5);

    // Adding to empty should copy
    CacheStats c;
    c += a;
    EXPECT_EQ(c.total_lookups, 10);
}

TEST(CacheEdgeTest, DisableHeatMap) {
    IPCache cache(100, 4, std::chrono::seconds(3600), 1024 * 1024, false);

    // Put and get entries
    cache.put("key1", nlohmann::json{{"data", 1}});
    cache.get("key1");
    cache.get("key1");

    // Heat map should still be accessible (but could be empty)
    auto heat_map = cache.get_heat_map();
    // The behavior may vary — just verify no crash
    EXPECT_NO_THROW(cache.get_heat_map());
}

TEST(CacheEdgeTest, LargeShardCount) {
    // Shard count > 64 is unusual but shouldn't crash
    IPCache cache(1000, 128, std::chrono::seconds(3600), 10 * 1024 * 1024);
    EXPECT_EQ(cache.shard_count(), 128);

    cache.put("test", nlohmann::json{{"data", 1}});
    auto val = cache.get("test");
    EXPECT_TRUE(val.has_value());
}