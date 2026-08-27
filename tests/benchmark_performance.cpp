#include <benchmark/benchmark.h>

#include <atomic>
#include <filesystem>
#include <future>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "auth.h"
#include "cache.h"
#include "database/asn_database.h"
#include "database/city_database.h"
#include "mac_database.h"
#include "rate_limiter.h"
#include "service/ip_geo_service.h"
#include "types.h"

using namespace ip_server;

// Helper function to find project root
std::filesystem::path find_project_root() {
    std::filesystem::path current_path = std::filesystem::current_path();

    if (std::filesystem::exists(current_path / "db")) return current_path;
    if (std::filesystem::exists(current_path / ".." / "db")) return current_path / "..";
    return current_path;
}

// ============================================================================
// MAC Database Benchmarks
// ============================================================================

class MACDatabaseBenchmark : public benchmark::Fixture {
   protected:
    void SetUp(::benchmark::State& state) override {
        auto project_root = find_project_root();
        oui_db_path       = project_root / "db" / "master_oui.db";

        if (!std::filesystem::exists(oui_db_path)) {
            state.SkipWithError("OUI database file not found");
            return;
        }

        oui_db_.open(oui_db_path.string());

        // Test MAC addresses
        test_macs_ = {"00:1A:2B:3C:4D:5E", "00:1A:2B:3C:4D:5F", "F4:EA:B5:12:34:56",
                      "00:0C:29:AB:CD:EF", "00:50:56:C0:00:08", "00:15:5D:00:00:01",
                      "00:1B:21:00:00:00", "00:1E:4F:00:00:00", "00:21:5C:00:00:00",
                      "00:24:81:00:00:00"};
    }

    void TearDown(const ::benchmark::State& /*state*/) override { oui_db_.close(); }

    std::filesystem::path oui_db_path;
    OUIDatabase oui_db_;
    std::vector<std::string> test_macs_;
};

BENCHMARK_F(MACDatabaseBenchmark, OUIDatabase_SingleLookup)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = oui_db_.lookup("00:1A:2B:3C:4D:5E");
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_F(MACDatabaseBenchmark, OUIDatabase_MultipleLookup)(benchmark::State& state) {
    size_t index = 0;
    for (auto _ : state) {
        auto result = oui_db_.lookup(test_macs_[index % test_macs_.size()]);
        benchmark::DoNotOptimize(result);
        index++;
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_F(MACDatabaseBenchmark, OUIDatabase_MACFormatVariants)(benchmark::State& state) {
    std::vector<std::string> mac_formats = {
        "00:1A:2B:3C:4D:5E",  // Colon
        "00-1A-2B-3C-4D-5E",  // Hyphen
        "001A2B3C4D5E",       // No separator
        "00:1a:2b:3c:4d:5e"   // Lowercase
    };
    size_t index = 0;
    for (auto _ : state) {
        auto result = oui_db_.lookup(mac_formats[index % mac_formats.size()]);
        benchmark::DoNotOptimize(result);
        index++;
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_F(MACDatabaseBenchmark, OUIDatabase_NotFoundLookup)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = oui_db_.lookup("FF:FF:FF:FF:FF:FF");
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations());
}

// ============================================================================
// Cache Performance Benchmarks
// ============================================================================

class CacheBenchmark : public benchmark::Fixture {
   protected:
    void SetUp(::benchmark::State& state) override {
        auto project_root = find_project_root();
        city_db_path      = project_root / "db" / "GeoLite2-City.mmdb";
        asn_db_path       = project_root / "db" / "GeoLite2-ASN.mmdb";

        if (!std::filesystem::exists(city_db_path) || !std::filesystem::exists(asn_db_path)) {
            state.SkipWithError("Database files not found");
            return;
        }

        city_db_.open(city_db_path.string());
        asn_db_.open(asn_db_path.string());

        // Generate test IPs
        generate_test_ips(1000);
    }

    void TearDown(const ::benchmark::State& /*state*/) override {
        city_db_.close();
        asn_db_.close();
    }

    void generate_test_ips(size_t count) {
        test_ips_.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            test_ips_.push_back(std::to_string((i >> 24) & 0xFF) + "."
                                + std::to_string((i >> 16) & 0xFF) + "."
                                + std::to_string((i >> 8) & 0xFF) + "." + std::to_string(i & 0xFF));
        }
    }

    std::filesystem::path city_db_path;
    std::filesystem::path asn_db_path;
    CityDatabase city_db_;
    ASNDatabase asn_db_;
    std::vector<std::string> test_ips_;
};

BENCHMARK_DEFINE_F(CacheBenchmark, Cache_GetHit)(benchmark::State& state) {
    IPCache cache(state.range(0));
    nlohmann::json dummy_result{{"test", "data"}};

    // Pre-populate cache
    for (const auto& ip : test_ips_) {
        cache.put(ip, dummy_result);
    }

    for (auto _ : state) {
        for (const auto& ip : test_ips_) {
            auto result = cache.get(ip);
            benchmark::DoNotOptimize(result);
        }
    }
    state.SetItemsProcessed(state.iterations() * test_ips_.size());
}
BENCHMARK_REGISTER_F(CacheBenchmark, Cache_GetHit)
    ->Args({100})
    ->Args({512})
    ->Args({4096})
    ->Args({10000});

BENCHMARK_DEFINE_F(CacheBenchmark, Cache_GetMiss)(benchmark::State& state) {
    IPCache cache(state.range(0));
    nlohmann::json dummy_result{{"test", "data"}};

    // Pre-populate with different IPs
    for (size_t i = 0; i < test_ips_.size(); ++i) {
        cache.put("192.168.0." + std::to_string(i), dummy_result);
    }

    for (auto _ : state) {
        for (const auto& ip : test_ips_) {
            auto result = cache.get(ip);
            benchmark::DoNotOptimize(result);
        }
    }
    state.SetItemsProcessed(state.iterations() * test_ips_.size());
}
BENCHMARK_REGISTER_F(CacheBenchmark, Cache_GetMiss)
    ->Args({100})
    ->Args({512})
    ->Args({4096})
    ->Args({10000});

BENCHMARK_DEFINE_F(CacheBenchmark, Cache_Put)(benchmark::State& state) {
    IPCache cache(state.range(0));
    nlohmann::json dummy_result{{"test", "data"}};

    for (auto _ : state) {
        for (size_t i = 0; i < test_ips_.size(); ++i) {
            cache.put(test_ips_[i], dummy_result);
        }
    }
    state.SetItemsProcessed(state.iterations() * test_ips_.size());
}
BENCHMARK_REGISTER_F(CacheBenchmark, Cache_Put)
    ->Args({100})
    ->Args({512})
    ->Args({4096})
    ->Args({10000});

BENCHMARK_DEFINE_F(CacheBenchmark, Cache_Eviction)(benchmark::State& state) {
    IPCache cache(state.range(0));
    nlohmann::json dummy_result{{"test", "data"}};

    for (auto _ : state) {
        cache.clear();
        for (size_t i = 0; i < test_ips_.size(); ++i) {
            cache.put(test_ips_[i], dummy_result);
        }
    }
    state.SetItemsProcessed(state.iterations() * test_ips_.size());
}
BENCHMARK_REGISTER_F(CacheBenchmark, Cache_Eviction)
    ->Args({100})
    ->Args({512})
    ->Args({4096})
    ->Args({10000});

// ============================================================================
// Rate Limiter Performance Benchmarks
// ============================================================================

class RateLimiterBenchmark : public benchmark::Fixture {
   protected:
    void SetUp(::benchmark::State& /*state*/) override {
        test_ips_.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            test_ips_.push_back("192.168.1." + std::to_string(i));
        }
    }

    std::vector<std::string> test_ips_;
};

BENCHMARK_F(RateLimiterBenchmark, RateLimiter_IsAllowed)(benchmark::State& state) {
    RateLimiter limiter(100, std::chrono::seconds(60), 10000);
    size_t index = 0;

    for (auto _ : state) {
        for (int i = 0; i < 100; ++i) {
            bool allowed = limiter.is_allowed(test_ips_[index % test_ips_.size()]);
            benchmark::DoNotOptimize(allowed);
            index++;
        }
    }
    state.SetItemsProcessed(state.iterations() * 100);
}

BENCHMARK_F(RateLimiterBenchmark, RateLimiter_GetRemaining)(benchmark::State& state) {
    RateLimiter limiter(100, std::chrono::seconds(60), 10000);
    size_t index = 0;

    // First allow some requests
    for (int i = 0; i < 1000; ++i) {
        limiter.is_allowed(test_ips_[i % test_ips_.size()]);
    }

    for (auto _ : state) {
        for (int i = 0; i < 100; ++i) {
            int remaining = limiter.get_remaining(test_ips_[index % test_ips_.size()]);
            benchmark::DoNotOptimize(remaining);
            index++;
        }
    }
    state.SetItemsProcessed(state.iterations() * 100);
}

BENCHMARK_F(RateLimiterBenchmark, RateLimiter_Cleanup)(benchmark::State& state) {
    RateLimiter limiter(10, std::chrono::seconds(1), 10000);

    // Create some rate-limited entries
    for (int round = 0; round < 10; ++round) {
        for (const auto& ip : test_ips_) {
            for (int i = 0; i < 10; ++i) {
                limiter.is_allowed(ip);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for (auto _ : state) {
        limiter.cleanup();
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_F(RateLimiterBenchmark, RateLimiter_MemoryStats)(benchmark::State& state) {
    RateLimiter limiter(100, std::chrono::seconds(60), 10000);

    // Create entries
    for (const auto& ip : test_ips_) {
        for (int i = 0; i < 50; ++i) {
            limiter.is_allowed(ip);
        }
    }

    for (auto _ : state) {
        auto stats = limiter.get_memory_stats();
        benchmark::DoNotOptimize(stats);
    }
    state.SetItemsProcessed(state.iterations());
}

// ============================================================================
// Batch Lookup Performance Benchmarks
// ============================================================================

class BatchLookupBenchmark : public benchmark::Fixture {
   protected:
    void SetUp(::benchmark::State& state) override {
        auto project_root = find_project_root();
        city_db_path      = project_root / "db" / "GeoLite2-City.mmdb";
        asn_db_path       = project_root / "db" / "GeoLite2-ASN.mmdb";

        if (!std::filesystem::exists(city_db_path) || !std::filesystem::exists(asn_db_path)) {
            state.SkipWithError("Database files not found");
            return;
        }

        service_ =
            std::make_unique<IPGeoService>(city_db_path.string(), asn_db_path.string(), 10000);
    }

    std::filesystem::path city_db_path;
    std::filesystem::path asn_db_path;
    std::unique_ptr<IPGeoService> service_;
};

BENCHMARK_DEFINE_F(BatchLookupBenchmark, BatchLookup_Sequential)(benchmark::State& state) {
    std::vector<std::string> batch_ips;
    batch_ips.reserve(state.range(0));

    for (int i = 0; i < state.range(0); ++i) {
        batch_ips.push_back(std::to_string((i >> 24) & 0xFF) + "."
                            + std::to_string((i >> 16) & 0xFF) + "."
                            + std::to_string((i >> 8) & 0xFF) + "." + std::to_string(i & 0xFF));
    }

    for (auto _ : state) {
        for (const auto& ip : batch_ips) {
            auto result = service_->lookup(ip);
            benchmark::DoNotOptimize(result);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK_REGISTER_F(BatchLookupBenchmark, BatchLookup_Sequential)->Range(10, 100);

BENCHMARK_DEFINE_F(BatchLookupBenchmark, BatchLookup_Parallel)(benchmark::State& state) {
    std::vector<std::string> batch_ips;
    batch_ips.reserve(state.range(0));

    for (int i = 0; i < state.range(0); ++i) {
        batch_ips.push_back(std::to_string((i >> 24) & 0xFF) + "."
                            + std::to_string((i >> 16) & 0xFF) + "."
                            + std::to_string((i >> 8) & 0xFF) + "." + std::to_string(i & 0xFF));
    }

    for (auto _ : state) {
        std::vector<std::future<LookupResult>> futures;
        futures.reserve(batch_ips.size());

        for (const auto& ip : batch_ips) {
            futures.push_back(
                std::async(std::launch::async, [this, &ip]() { return service_->lookup(ip); }));
        }

        for (auto& future : futures) {
            auto result = future.get();
            benchmark::DoNotOptimize(result);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK_REGISTER_F(BatchLookupBenchmark, BatchLookup_Parallel)->Range(10, 100);

BENCHMARK_F(BatchLookupBenchmark, BatchLookup_ThreadPool)(benchmark::State& state) {
    std::vector<std::string> batch_ips;
    batch_ips.reserve(50);

    for (int i = 0; i < 50; ++i) {
        batch_ips.push_back(std::to_string((i >> 24) & 0xFF) + "."
                            + std::to_string((i >> 16) & 0xFF) + "."
                            + std::to_string((i >> 8) & 0xFF) + "." + std::to_string(i & 0xFF));
    }

    std::vector<std::thread> threads;
    std::atomic<size_t> index{0};
    std::vector<LookupResult> results;
    results.resize(batch_ips.size());

    for (auto _ : state) {
        index = 0;
        threads.clear();

        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([this, &batch_ips, &results, &index]() {
                while (true) {
                    size_t i = index++;
                    if (i >= batch_ips.size()) break;
                    results[i] = service_->lookup(batch_ips[i]);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        for (auto& result : results) {
            benchmark::DoNotOptimize(result);
        }
    }
    state.SetItemsProcessed(state.iterations() * batch_ips.size());
}

// ============================================================================
// Memory Usage Benchmarks
// ============================================================================

class MemoryBenchmark : public benchmark::Fixture {
   protected:
    void SetUp(::benchmark::State& state) override {
        auto project_root = find_project_root();
        city_db_path      = project_root / "db" / "GeoLite2-City.mmdb";
        asn_db_path       = project_root / "db" / "GeoLite2-ASN.mmdb";

        if (!std::filesystem::exists(city_db_path) || !std::filesystem::exists(asn_db_path)) {
            state.SkipWithError("Database files not found");
            return;
        }
    }

    std::filesystem::path city_db_path;
    std::filesystem::path asn_db_path;
};

BENCHMARK_DEFINE_F(MemoryBenchmark, Memory_IPServiceWithCache)(benchmark::State& state) {
    for (auto _ : state) {
        IPGeoService service(city_db_path.string(), asn_db_path.string(), state.range(0));
        benchmark::DoNotOptimize(service);

        // Pre-warm cache
        for (int i = 0; i < static_cast<int>(state.range(0)); ++i) {
            service.lookup(std::to_string(i % 256) + "." + std::to_string(i % 256) + "."
                           + std::to_string(i % 256) + "." + std::to_string(i % 256));
        }
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(MemoryBenchmark, Memory_IPServiceWithCache)
    ->Args({1000})
    ->Args({10000})
    ->Args({50000});

BENCHMARK_DEFINE_F(MemoryBenchmark, Memory_RateLimiterWithEntries)(benchmark::State& state) {
    for (auto _ : state) {
        RateLimiter limiter(100, std::chrono::seconds(60), 100000);

        // Add entries
        for (int i = 0; i < state.range(0); ++i) {
            limiter.is_allowed("192.168.1." + std::to_string(i));
        }

        auto stats = limiter.get_memory_stats();
        benchmark::DoNotOptimize(stats);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(MemoryBenchmark, Memory_RateLimiterWithEntries)
    ->Args({100})
    ->Args({1000})
    ->Args({10000});

// ============================================================================
// API Auth Performance Benchmarks
// ============================================================================

class AuthBenchmark : public benchmark::Fixture {};

BENCHMARK_F(AuthBenchmark, APIAuth_IsValid)(benchmark::State& state) {
    APIAuth auth(true);

    // Add keys
    for (int i = 0; i < 1000; ++i) {
        auth.add_key("test_key_" + std::to_string(i));
    }

    for (auto _ : state) {
        for (int i = 0; i < 100; ++i) {
            bool valid = auth.is_valid("test_key_" + std::to_string(i % 1000));
            benchmark::DoNotOptimize(valid);
        }
    }
    state.SetItemsProcessed(state.iterations() * 100);
}

BENCHMARK_F(AuthBenchmark, APIAuth_LoadKeysFromFile)(benchmark::State& state) {
    for (auto _ : state) {
        APIAuth auth(true);
        auth.add_key("test_key_1");
        auth.add_key("test_key_2");
        benchmark::DoNotOptimize(auth);
    }
    state.SetItemsProcessed(state.iterations());
}

// ============================================================================
// JSON Serialization Benchmarks
// ============================================================================

class JSONBenchmark : public benchmark::Fixture {};

BENCHMARK_F(JSONBenchmark, JSON_Serialization)(benchmark::State& state) {
    nlohmann::json test_data = {{"ip", "8.8.8.8"},
                                {"found", true},
                                {"country", "United States"},
                                {"country_code", "US"},
                                {"city", "Mountain View"},
                                {"continent", "North America"},
                                {"latitude", 37.4223},
                                {"longitude", -122.085},
                                {"timezone", "America/Los_Angeles"},
                                {"as_organization", "Google LLC"},
                                {"as_number", 15169}};

    for (auto _ : state) {
        auto str = test_data.dump();
        benchmark::DoNotOptimize(str);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_F(JSONBenchmark, JSON_Deserialization)(benchmark::State& state) {
    std::string json_str =
        R"({"ip":"8.8.8.8","found":true,"country":"United States","country_code":"US","city":"Mountain View","latitude":37.4223,"longitude":-122.085})";

    for (auto _ : state) {
        auto j = nlohmann::json::parse(json_str);
        benchmark::DoNotOptimize(j);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_F(JSONBenchmark, JSON_LargeSerialization)(benchmark::State& state) {
    nlohmann::json test_data = nlohmann::json::array();

    for (int i = 0; i < 100; ++i) {
        test_data.push_back({{"ip", "8.8.8." + std::to_string(i)},
                             {"found", true},
                             {"country", "United States"},
                             {"city", "City " + std::to_string(i)}});
    }

    for (auto _ : state) {
        auto str = test_data.dump();
        benchmark::DoNotOptimize(str);
    }
    state.SetItemsProcessed(state.iterations());
}

// ============================================================================
// Concurrent Access Benchmarks
// ============================================================================

class ConcurrentBenchmark : public benchmark::Fixture {
   protected:
    void SetUp(::benchmark::State& state) override {
        auto project_root = find_project_root();
        city_db_path      = project_root / "db" / "GeoLite2-City.mmdb";
        asn_db_path       = project_root / "db" / "GeoLite2-ASN.mmdb";

        if (!std::filesystem::exists(city_db_path) || !std::filesystem::exists(asn_db_path)) {
            state.SkipWithError("Database files not found");
            return;
        }

        service_ =
            std::make_unique<IPGeoService>(city_db_path.string(), asn_db_path.string(), 10000);
    }

    void TearDown(const ::benchmark::State& /*state*/) override {
        service_.reset();  // Explicitly destroy service while Logger is still alive
    }

    std::filesystem::path city_db_path;
    std::filesystem::path asn_db_path;
    std::unique_ptr<IPGeoService> service_;
};

BENCHMARK_DEFINE_F(ConcurrentBenchmark, Concurrent_IPLookup)(benchmark::State& state) {
    std::vector<std::string> test_ips;
    for (int i = 0; i < 100; ++i) {
        test_ips.push_back(std::to_string((i >> 24) & 0xFF) + "." + std::to_string((i >> 16) & 0xFF)
                           + "." + std::to_string((i >> 8) & 0xFF) + "."
                           + std::to_string(i & 0xFF));
    }

    std::atomic<uint64_t> total_lookups{0};
    std::vector<std::thread> threads;
    int num_threads = state.range(0);

    for (auto _ : state) {
        threads.clear();
        total_lookups = 0;

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([this, &test_ips, &total_lookups, t, num_threads]() {
                size_t index = t * (test_ips.size() / num_threads);
                size_t end   = (t + 1) * (test_ips.size() / num_threads);
                for (size_t i = index; i < end; ++i) {
                    auto result = service_->lookup(test_ips[i % test_ips.size()]);
                    benchmark::DoNotOptimize(result);
                    total_lookups.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    state.SetItemsProcessed(total_lookups.load());
}
BENCHMARK_REGISTER_F(ConcurrentBenchmark, Concurrent_IPLookup)
    ->Args({1})
    ->Args({2})
    ->Args({4})
    ->Args({8})
    ->Args({16});

// Run all benchmarks
// ============================================================================
// Sharded Cache Concurrent Performance Benchmarks
// ============================================================================

class ShardedCacheBenchmark : public benchmark::Fixture {
   protected:
    void SetUp(::benchmark::State& state) override {
        // Test with different shard counts
        shard_count_ = state.range(0);
        cache_size_  = state.range(1);

        // Generate test IPs
        for (int i = 0; i < 1000; ++i) {
            test_ips_.push_back(std::to_string((i >> 24) & 0xFF) + "."
                                + std::to_string((i >> 16) & 0xFF) + "."
                                + std::to_string((i >> 8) & 0xFF) + "." + std::to_string(i & 0xFF));
        }

        dummy_result_ = {{"ip", "8.8.8.8"},
                         {"found", true},
                         {"country", "United States"},
                         {"country_code", "US"},
                         {"city", "Mountain View"}};
    }

    std::vector<std::string> test_ips_;
    nlohmann::json dummy_result_;
    size_t shard_count_;
    size_t cache_size_;
};

BENCHMARK_DEFINE_F(ShardedCacheBenchmark, ShardedCache_ConcurrentRead)(benchmark::State& state) {
    IPCache cache(cache_size_, shard_count_);

    // Pre-populate cache
    for (const auto& ip : test_ips_) {
        cache.put(ip, dummy_result_);
    }

    std::atomic<uint64_t> total_reads{0};
    std::vector<std::thread> threads;
    int num_threads = 8;

    for (auto _ : state) {
        threads.clear();
        total_reads = 0;

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([this, &cache, &total_reads, t, num_threads]() {
                size_t index = t * (test_ips_.size() / num_threads);
                size_t end   = (t + 1) * (test_ips_.size() / num_threads);
                for (size_t i = index; i < end; ++i) {
                    auto result = cache.get(test_ips_[i]);
                    benchmark::DoNotOptimize(result);
                    total_reads.fetch_add(1);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    state.SetItemsProcessed(state.iterations() * test_ips_.size());
    state.counters["shards"]     = shard_count_;
    state.counters["cache_size"] = cache_size_;
}
BENCHMARK_REGISTER_F(ShardedCacheBenchmark, ShardedCache_ConcurrentRead)
    ->Args({1, 1000})
    ->Args({4, 1000})
    ->Args({8, 1000})
    ->Args({16, 1000});

BENCHMARK_DEFINE_F(ShardedCacheBenchmark, ShardedCache_ConcurrentWrite)(benchmark::State& state) {
    IPCache cache(cache_size_, shard_count_);

    std::atomic<uint64_t> total_writes{0};
    std::vector<std::thread> threads;
    int num_threads = 8;

    for (auto _ : state) {
        threads.clear();
        total_writes = 0;

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([this, &cache, &total_writes, t, num_threads]() {
                size_t index = t * (test_ips_.size() / num_threads);
                size_t end   = (t + 1) * (test_ips_.size() / num_threads);
                for (size_t i = index; i < end; ++i) {
                    cache.put(test_ips_[i], dummy_result_);
                    total_writes.fetch_add(1);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    state.SetItemsProcessed(state.iterations() * test_ips_.size());
    state.counters["shards"]     = shard_count_;
    state.counters["cache_size"] = cache_size_;
}
BENCHMARK_REGISTER_F(ShardedCacheBenchmark, ShardedCache_ConcurrentWrite)
    ->Args({1, 1000})
    ->Args({4, 1000})
    ->Args({8, 1000})
    ->Args({16, 1000});

BENCHMARK_DEFINE_F(ShardedCacheBenchmark, ShardedCache_MixedWorkload)(benchmark::State& state) {
    IPCache cache(cache_size_, shard_count_);

    // Pre-populate half the cache
    for (size_t i = 0; i < test_ips_.size() / 2; ++i) {
        cache.put(test_ips_[i], dummy_result_);
    }

    std::atomic<uint64_t> total_ops{0};
    std::vector<std::thread> threads;
    int num_threads = 8;

    for (auto _ : state) {
        threads.clear();
        total_ops = 0;

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([this, &cache, &total_ops, t, num_threads]() {
                size_t index = t * (test_ips_.size() / num_threads);
                size_t end   = (t + 1) * (test_ips_.size() / num_threads);
                for (size_t i = index; i < end; ++i) {
                    // 70% reads, 30% writes
                    if (i % 10 < 7) {
                        auto result = cache.get(test_ips_[i]);
                        benchmark::DoNotOptimize(result);
                    } else {
                        cache.put(test_ips_[i], dummy_result_);
                    }
                    total_ops.fetch_add(1);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    state.SetItemsProcessed(state.iterations() * test_ips_.size());
    state.counters["shards"]     = shard_count_;
    state.counters["cache_size"] = cache_size_;
}
BENCHMARK_REGISTER_F(ShardedCacheBenchmark, ShardedCache_MixedWorkload)
    ->Args({1, 1000})
    ->Args({4, 1000})
    ->Args({8, 1000})
    ->Args({16, 1000});

// ============================================================================
// Cache Hit Rate Benchmarks
// ============================================================================

class CacheHitRateBenchmark : public benchmark::Fixture {
   protected:
    void SetUp(::benchmark::State& state) override {
        project_root = find_project_root();
        city_db_path = project_root / "db" / "GeoLite2-City.mmdb";
        asn_db_path  = project_root / "db" / "GeoLite2-ASN.mmdb";

        if (!std::filesystem::exists(city_db_path) || !std::filesystem::exists(asn_db_path)) {
            state.SkipWithError("Database files not found");
            return;
        }

        service_ =
            std::make_unique<IPGeoService>(city_db_path.string(), asn_db_path.string(), 10000);

        // Use known public DNS servers
        test_ips_ = {"8.8.8.8",   "1.1.1.1",   "9.9.9.9",       "208.67.222.222", "208.67.220.220",
                     "64.6.64.6", "64.6.65.6", "185.228.168.9", "185.228.169.9",  "1.0.0.1"};
    }

    void TearDown(const ::benchmark::State& /*state*/) override {
        service_.reset();  // Explicitly destroy service while Logger is still alive
    }

    std::filesystem::path project_root;
    std::filesystem::path city_db_path;
    std::filesystem::path asn_db_path;
    std::unique_ptr<IPGeoService> service_;
    std::vector<std::string> test_ips_;
};

BENCHMARK_DEFINE_F(CacheHitRateBenchmark, CacheHitRate_90Percent)(benchmark::State& state) {
    // 90% cache hit rate simulation
    size_t hot_ips = test_ips_.size();
    size_t hits    = 0;

    for (auto _ : state) {
        for (size_t i = 0; i < 100; ++i) {
            // 90% hot, 10% cold
            if (i < 90) {
                hits += service_->lookup(test_ips_[i % hot_ips]).cache_hit_;
            } else {
                std::string cold_ip =
                    "10.0." + std::to_string((i / 256) % 256) + "." + std::to_string(i % 256);
                hits += service_->lookup(cold_ip).cache_hit_;
            }
        }
    }

    state.SetItemsProcessed(state.iterations() * 100);
    state.counters["hit_rate"] = hits * 100.0 / (static_cast<double>(state.iterations()) * 100.0);
}
BENCHMARK_REGISTER_F(CacheHitRateBenchmark, CacheHitRate_90Percent);

BENCHMARK_DEFINE_F(CacheHitRateBenchmark, CacheHitRate_95Percent)(benchmark::State& state) {
    // 95% cache hit rate simulation
    size_t hot_ips = test_ips_.size();
    size_t hits    = 0;

    for (auto _ : state) {
        for (size_t i = 0; i < 100; ++i) {
            // 95% hot, 5% cold
            if (i < 95) {
                hits += service_->lookup(test_ips_[i % hot_ips]).cache_hit_;
            } else {
                std::string cold_ip =
                    "10.0." + std::to_string((i / 256) % 256) + "." + std::to_string(i % 256);
                hits += service_->lookup(cold_ip).cache_hit_;
            }
        }
    }

    state.SetItemsProcessed(state.iterations() * 100);
    state.counters["hit_rate"] = hits * 100.0 / (static_cast<double>(state.iterations()) * 100.0);
}
BENCHMARK_REGISTER_F(CacheHitRateBenchmark, CacheHitRate_95Percent);

BENCHMARK_DEFINE_F(CacheHitRateBenchmark, CacheHitRate_99Percent)(benchmark::State& state) {
    // 99% cache hit rate simulation
    size_t hot_ips = test_ips_.size();
    size_t hits    = 0;

    for (auto _ : state) {
        for (size_t i = 0; i < 100; ++i) {
            // 99% hot, 1% cold
            if (i < 99) {
                hits += service_->lookup(test_ips_[i % hot_ips]).cache_hit_;
            } else {
                std::string cold_ip =
                    "10.0." + std::to_string((i / 256) % 256) + "." + std::to_string(i % 256);
                hits += service_->lookup(cold_ip).cache_hit_;
            }
        }
    }

    state.SetItemsProcessed(state.iterations() * 100);
    state.counters["hit_rate"] = hits * 100.0 / (static_cast<double>(state.iterations()) * 100.0);
}
BENCHMARK_REGISTER_F(CacheHitRateBenchmark, CacheHitRate_99Percent);

BENCHMARK_MAIN();