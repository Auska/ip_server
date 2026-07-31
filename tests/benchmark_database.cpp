#include <benchmark/benchmark.h>

#include <filesystem>
#include <string>
#include <vector>

#include "database/asn_database.h"
#include "database/city_database.h"
#include "service/ip_geo_service.h"
#include "types.h"

using namespace ip_server;

// Benchmark fixture
class DatabaseBenchmark : public benchmark::Fixture {
   protected:
    void SetUp(::benchmark::State& state) override {
        // Get the project root directory
        std::filesystem::path current_path = std::filesystem::current_path();
        std::filesystem::path project_root;

        if (std::filesystem::exists(current_path / "db"))
            project_root = current_path;
        else if (std::filesystem::exists(current_path / ".." / "db"))
            project_root = current_path / "..";
        else
            project_root = current_path;

        city_db_path = project_root / "db" / "GeoLite2-City.mmdb";
        asn_db_path  = project_root / "db" / "GeoLite2-ASN.mmdb";

        // Skip if database files don't exist
        if (!std::filesystem::exists(city_db_path) || !std::filesystem::exists(asn_db_path)) {
            state.SkipWithError("Database files not found");
            return;
        }

        // Open databases
        city_db_.open(city_db_path.string());
        asn_db_.open(asn_db_path.string());

        // Prepare test IPs
        test_ips_ = {
            "8.8.8.8",               // Google DNS
            "1.1.1.1",               // Cloudflare DNS
            "114.114.114.114",       // Chinese DNS
            "208.67.222.222",        // OpenDNS
            "9.9.9.9",               // Quad9 DNS
            "64.6.64.6",             // Verisign DNS
            "84.200.69.80",          // DNS.WATCH
            "91.239.100.100",        // UncensoredDNS
            "185.228.168.9",         // CleanBrowsing
            "10.0.0.1",              // Private IP
            "192.168.1.1",           // Private IP
            "172.16.0.1",            // Private IP
            "2001:4860:4860::8888",  // Google DNS IPv6
            "2606:4700:4700::1111"   // Cloudflare DNS IPv6
        };
    }

    void TearDown(const ::benchmark::State& /*state*/) override {
        city_db_.close();
        asn_db_.close();
    }

    std::filesystem::path city_db_path;
    std::filesystem::path asn_db_path;
    CityDatabase city_db_;
    ASNDatabase asn_db_;
    std::vector<std::string> test_ips_;
};

// Benchmark CityDatabase lookup for a single IP
BENCHMARK_F(DatabaseBenchmark, CityDatabase_SingleLookup)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = city_db_.lookup("8.8.8.8");
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations());
}

// Benchmark ASNDatabase lookup for a single IP
BENCHMARK_F(DatabaseBenchmark, ASNDatabase_SingleLookup)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = asn_db_.lookup("8.8.8.8");
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations());
}

// Benchmark CityDatabase lookup with multiple IPs
BENCHMARK_F(DatabaseBenchmark, CityDatabase_MultipleLookup)(benchmark::State& state) {
    size_t index = 0;
    for (auto _ : state) {
        auto result = city_db_.lookup(test_ips_[index % test_ips_.size()]);
        benchmark::DoNotOptimize(result);
        index++;
    }
    state.SetItemsProcessed(state.iterations());
}

// Benchmark ASNDatabase lookup with multiple IPs
BENCHMARK_F(DatabaseBenchmark, ASNDatabase_MultipleLookup)(benchmark::State& state) {
    size_t index = 0;
    for (auto _ : state) {
        auto result = asn_db_.lookup(test_ips_[index % test_ips_.size()]);
        benchmark::DoNotOptimize(result);
        index++;
    }
    state.SetItemsProcessed(state.iterations());
}

// Benchmark IPGeoService lookup with repeated IPs (cache hit)
BENCHMARK_F(DatabaseBenchmark, IPGeoService_CacheHit)(benchmark::State& state) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 10000);

    // Pre-warm cache
    for (const auto& ip : test_ips_) {
        service.lookup(ip);
    }

    size_t index = 0;
    for (auto _ : state) {
        // All lookups will hit the cache
        auto result = service.lookup(test_ips_[index % test_ips_.size()]);
        benchmark::DoNotOptimize(result);
        index++;
    }
    state.SetItemsProcessed(state.iterations());
}

// Benchmark CityDatabase open/close
BENCHMARK_F(DatabaseBenchmark, CityDatabase_OpenClose)(benchmark::State& state) {
    for (auto _ : state) {
        CityDatabase db;
        db.open(city_db_path.string());
        benchmark::DoNotOptimize(db);
        db.close();
    }
    state.SetItemsProcessed(state.iterations());
}

// Benchmark ASNDatabase open/close
BENCHMARK_F(DatabaseBenchmark, ASNDatabase_OpenClose)(benchmark::State& state) {
    for (auto _ : state) {
        ASNDatabase db;
        db.open(asn_db_path.string());
        benchmark::DoNotOptimize(db);
        db.close();
    }
    state.SetItemsProcessed(state.iterations());
}

// Benchmark IPGeoService initialization
BENCHMARK_F(DatabaseBenchmark, IPGeoService_Initialization)(benchmark::State& state) {
    for (auto _ : state) {
        IPGeoService service(city_db_path.string(), asn_db_path.string(), 10000);
        benchmark::DoNotOptimize(service);
    }
    state.SetItemsProcessed(state.iterations());
}

// Benchmark lookup performance with IPv6 addresses
BENCHMARK_F(DatabaseBenchmark, CityDatabase_IPv6Lookup)(benchmark::State& state) {
    std::vector<std::string> ipv6_addresses = {"2001:4860:4860::8888", "2606:4700:4700::1111",
                                               "2001:1608:10:25::9249:d69b", "2620:fe::fe",
                                               "2001:4860:4860::8844"};

    size_t index = 0;
    for (auto _ : state) {
        auto result = city_db_.lookup(ipv6_addresses[index % ipv6_addresses.size()]);
        benchmark::DoNotOptimize(result);
        index++;
    }
    state.SetItemsProcessed(state.iterations());
}

// Benchmark concurrent lookups (simulated)
BENCHMARK_F(DatabaseBenchmark, IPGeoService_ConcurrentLookups)(benchmark::State& state) {
    IPGeoService service(city_db_path.string(), asn_db_path.string(), 10000);

    size_t index = 0;
    for (auto _ : state) {
        // Simulate multiple concurrent lookups (5 at a time)
        for (int i = 0; i < 5; ++i) {
            auto result = service.lookup(test_ips_[(index + i) % test_ips_.size()]);
            benchmark::DoNotOptimize(result);
        }
        index += 5;
    }
    state.SetItemsProcessed(state.iterations() * 5);
}

// Run the benchmarks
// BENCHMARK_MAIN();  // Moved to benchmark_performance.cpp to avoid multiple
// main()