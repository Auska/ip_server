#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "database.h"
#include "http_server.h"

using namespace ip_server;

class HTTPServerTest : public ::testing::Test {
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
                         << " and " << asn_db_path.string() << ". Skipping HTTP server tests.";
        }

        // Initialize service
        service = std::make_unique<IPGeoService>(city_db_path.string(), asn_db_path.string());

        // Start server on a random port
        test_port = 18080;
        server    = std::make_unique<IPGeoHTTPServer>("127.0.0.1", test_port);

        server->set_lookup_handler([this](const std::string& ip) { return service->lookup(ip); });

        // Start server in background thread
        shutdown_requested.store(false);
        server_thread = std::thread([this]() { server->start(shutdown_requested); });

        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        if (server) {
            shutdown_requested.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // Note: server->stop() is NOT called here because the start() method
            // already handles stopping the server internally when shutdown_requested
            // is set. Calling stop() again would cause assertion failures.
        }
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }

    std::filesystem::path city_db_path;
    std::filesystem::path asn_db_path;
    std::unique_ptr<IPGeoService> service;
    std::unique_ptr<IPGeoHTTPServer> server;
    std::thread server_thread;
    uint16_t test_port;
    std::atomic<bool> shutdown_requested;
};

TEST_F(HTTPServerTest, RootEndpoint) {
    httplib::Client client("127.0.0.1", test_port);
    auto result = client.Get("/");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.contains("service"));
    EXPECT_TRUE(json.contains("version"));
    EXPECT_TRUE(json.contains("endpoints"));
}

TEST_F(HTTPServerTest, HealthEndpoint) {
    httplib::Client client("127.0.0.1", test_port);
    auto result = client.Get("/health");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_EQ(json["status"], "ok");
    EXPECT_TRUE(json.contains("timestamp"));
}

TEST_F(HTTPServerTest, LookupEndpointValidIP) {
    httplib::Client client("127.0.0.1", test_port);
    auto result = client.Get("/lookup?ip=8.8.8.8");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_EQ(json["ip"], "8.8.8.8");
    EXPECT_TRUE(json.contains("found"));
}

TEST_F(HTTPServerTest, LookupEndpointMissingIP) {
    httplib::Client client("127.0.0.1", test_port);
    auto result = client.Get("/lookup");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.contains("ip"));
    EXPECT_EQ(json["ip"], "127.0.0.1");
}

TEST_F(HTTPServerTest, LookupEndpointInvalidIP) {
    httplib::Client client("127.0.0.1", test_port);
    auto result = client.Get("/lookup?ip=invalid.ip");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.contains("ip"));
    EXPECT_TRUE(json.contains("error"));
}

TEST_F(HTTPServerTest, BatchLookupEndpoint) {
    httplib::Client client("127.0.0.1", test_port);
    nlohmann::json request_body;
    request_body["ips"] = {"8.8.8.8", "1.1.1.1"};

    auto result = client.Post("/lookup", request_body.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 2);
}

TEST_F(HTTPServerTest, BatchLookupEndpointMissingIps) {
    httplib::Client client("127.0.0.1", test_port);
    nlohmann::json request_body;
    request_body["invalid"] = "data";

    auto result = client.Post("/lookup", request_body.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.contains("error"));
}

TEST_F(HTTPServerTest, BatchLookupEndpointInvalidJson) {
    httplib::Client client("127.0.0.1", test_port);
    auto result = client.Post("/lookup", "invalid json", "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.contains("error"));
}

TEST_F(HTTPServerTest, CORSSupport) {
    httplib::Client client("127.0.0.1", test_port);
    httplib::Headers headers = {{"Origin", "http://example.com"}};

    auto result = client.Get("/lookup?ip=8.8.8.8", headers);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto it = result->headers.find("Access-Control-Allow-Origin");
    EXPECT_NE(it, result->headers.end());
    EXPECT_EQ(it->second, "*");
}

TEST_F(HTTPServerTest, OptionsRequest) {
    httplib::Client client("127.0.0.1", test_port);
    auto result = client.Options("/lookup");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
}

TEST_F(HTTPServerTest, MultipleConcurrentRequests) {
    httplib::Client client("127.0.0.1", test_port);

    std::vector<std::future<httplib::Result>> futures;

    for (int i = 0; i < 10; i++) {
        futures.push_back(std::async(std::launch::async,
                                     [&client]() { return client.Get("/lookup?ip=8.8.8.8"); }));
    }

    for (auto& future : futures) {
        auto result = future.get();
        ASSERT_TRUE(result);
        EXPECT_EQ(result->status, 200);
    }
}

TEST_F(HTTPServerTest, HTTPServerNotCopyable) {
    EXPECT_FALSE(std::is_copy_constructible<IPGeoHTTPServer>::value);
    EXPECT_FALSE(std::is_copy_assignable<IPGeoHTTPServer>::value);
}

// Test rate limiting functionality
TEST_F(HTTPServerTest, RateLimiting) {
    // Create a new server with strict rate limiting (3 requests per minute)
    uint16_t rate_limit_port = 18081;
    auto rate_limited_server =
        std::make_unique<IPGeoHTTPServer>("127.0.0.1", rate_limit_port, 4, true, 3);

    rate_limited_server->set_lookup_handler(
        [this](const std::string& ip) { return service->lookup(ip); });

    // Start server in background thread
    std::atomic<bool> rl_shutdown_requested(false);
    std::thread rate_limit_thread([&rate_limited_server, &rl_shutdown_requested]() {
        rate_limited_server->start(rl_shutdown_requested);
    });

    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    httplib::Client client("127.0.0.1", rate_limit_port);

    // Make 5 requests (limit is 3)
    int success_count      = 0;
    int rate_limited_count = 0;

    for (int i = 0; i < 5; i++) {
        auto result = client.Get("/lookup?ip=8.8.8.8");
        ASSERT_TRUE(result);

        if (result->status == 200) {
            success_count++;
        } else if (result->status == 429) {
            rate_limited_count++;
            // Verify rate limit response
            auto json = nlohmann::json::parse(result->body);
            EXPECT_TRUE(json.contains("error"));
            EXPECT_EQ(json["error"], "Rate limit exceeded");
            EXPECT_TRUE(json.contains("remaining"));
        }
    }

    EXPECT_GE(success_count, 3);       // At least 3 requests should succeed
    EXPECT_GE(rate_limited_count, 1);  // At least 1 should be rate limited

    // Cleanup
    rl_shutdown_requested.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    rate_limited_server->stop();
    rate_limit_thread.join();
}

// Test batch size limit
TEST_F(HTTPServerTest, BatchSizeLimit) {
    // Create a new server with small batch size limit (3 IPs)
    uint16_t batch_limit_port = 18082;
    auto batch_limited_server =
        std::make_unique<IPGeoHTTPServer>("127.0.0.1", batch_limit_port, 4, false, 100, 3);

    batch_limited_server->set_lookup_handler(
        [this](const std::string& ip) { return service->lookup(ip); });

    // Start server in background thread
    std::atomic<bool> bl_shutdown_requested(false);
    std::thread batch_limit_thread([&batch_limited_server, &bl_shutdown_requested]() {
        batch_limited_server->start(bl_shutdown_requested);
    });

    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    httplib::Client client("127.0.0.1", batch_limit_port);

    // Test with batch size exceeding limit (5 IPs, limit is 3)
    nlohmann::json large_batch;
    large_batch["ips"] = {"8.8.8.8", "1.1.1.1", "9.9.9.9", "8.8.4.4", "1.0.0.1"};

    auto result = client.Post("/lookup", large_batch.dump(), "application/json");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 400);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "Batch size exceeds maximum limit");
    EXPECT_TRUE(json.contains("max_batch_size"));
    EXPECT_EQ(json["max_batch_size"], 3);
    EXPECT_TRUE(json.contains("requested_size"));
    EXPECT_EQ(json["requested_size"], 5);

    // Test with batch size within limit (2 IPs)
    nlohmann::json small_batch;
    small_batch["ips"] = {"8.8.8.8", "1.1.1.1"};

    result = client.Post("/lookup", small_batch.dump(), "application/json");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 2);

    // Cleanup
    bl_shutdown_requested.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    batch_limited_server->stop();
    batch_limit_thread.join();
}

// Test rate limiting with batch requests
TEST_F(HTTPServerTest, RateLimitingWithBatchRequests) {
    // Create a new server with strict rate limiting (2 requests per minute)
    uint16_t rate_limit_batch_port = 18083;
    auto rate_limited_server =
        std::make_unique<IPGeoHTTPServer>("127.0.0.1", rate_limit_batch_port, 4, true, 2);

    rate_limited_server->set_lookup_handler(
        [this](const std::string& ip) { return service->lookup(ip); });

    // Start server in background thread
    std::atomic<bool> rlb_shutdown_requested(false);
    std::thread rate_limit_thread([&rate_limited_server, &rlb_shutdown_requested]() {
        rate_limited_server->start(rlb_shutdown_requested);
    });

    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    httplib::Client client("127.0.0.1", rate_limit_batch_port);

    // Make 3 batch requests (limit is 2)
    int success_count      = 0;
    int rate_limited_count = 0;

    for (int i = 0; i < 3; i++) {
        nlohmann::json request_body;
        request_body["ips"] = {"8.8.8.8", "1.1.1.1"};

        auto result = client.Post("/lookup", request_body.dump(), "application/json");
        ASSERT_TRUE(result);

        if (result->status == 200) {
            success_count++;
        } else if (result->status == 429) {
            rate_limited_count++;
        }
    }

    EXPECT_GE(success_count, 2);       // At least 2 requests should succeed
    EXPECT_GE(rate_limited_count, 1);  // At least 1 should be rate limited

    // Cleanup
    rlb_shutdown_requested.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    rate_limited_server->stop();
    rate_limit_thread.join();
}

// Test disabled rate limiter
TEST_F(HTTPServerTest, DisabledRateLimiter) {
    // Create a new server with rate limiter disabled
    uint16_t no_rate_limit_port = 18084;
    auto no_rate_limit_server =
        std::make_unique<IPGeoHTTPServer>("127.0.0.1", no_rate_limit_port, 4, false);

    no_rate_limit_server->set_lookup_handler(
        [this](const std::string& ip) { return service->lookup(ip); });

    // Start server in background thread
    std::atomic<bool> nrl_shutdown_requested(false);
    std::thread no_rate_limit_thread([&no_rate_limit_server, &nrl_shutdown_requested]() {
        no_rate_limit_server->start(nrl_shutdown_requested);
    });

    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    httplib::Client client("127.0.0.1", no_rate_limit_port);

    // Make 10 requests (should all succeed since rate limiting is disabled)
    for (int i = 0; i < 10; i++) {
        auto result = client.Get("/lookup?ip=8.8.8.8");
        ASSERT_TRUE(result);
        EXPECT_EQ(result->status, 200);
    }

    // Cleanup
    nrl_shutdown_requested.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    no_rate_limit_server->stop();
    no_rate_limit_thread.join();
}

// Test parallel batch lookup performance
TEST_F(HTTPServerTest, ParallelBatchLookupPerformance) {
    httplib::Client client("127.0.0.1", test_port);

    // Create a batch of 20 unique IPs using a less common range to avoid cache from previous tests
    nlohmann::json batch_request;
    std::vector<std::string> test_ips;
    for (int i = 0; i < 20; ++i) {
        std::string ip = "93.184.216." + std::to_string(100 + i);  // Using example.com IP range
        test_ips.push_back(ip);
        batch_request["ips"].push_back(ip);
    }

    // First batch - all cache misses
    auto start   = std::chrono::high_resolution_clock::now();
    auto result1 = client.Post("/lookup", batch_request.dump(), "application/json");
    auto first_batch_time =
        std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start)
            .count();

    ASSERT_TRUE(result1);
    EXPECT_EQ(result1->status, 200);

    auto json1 = nlohmann::json::parse(result1->body);
    EXPECT_TRUE(json1.is_array());
    EXPECT_EQ(json1.size(), 20);

    // Verify all results
    for (size_t i = 0; i < json1.size(); ++i) {
        EXPECT_TRUE(json1[i].contains("ip"));
        EXPECT_EQ(json1[i]["ip"], test_ips[i]);
        EXPECT_TRUE(json1[i].contains("found"));
    }

    // Second batch - all cache hits
    start        = std::chrono::high_resolution_clock::now();
    auto result2 = client.Post("/lookup", batch_request.dump(), "application/json");
    auto second_batch_time =
        std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start)
            .count();

    ASSERT_TRUE(result2);
    EXPECT_EQ(result2->status, 200);

    auto json2 = nlohmann::json::parse(result2->body);
    EXPECT_TRUE(json2.is_array());
    EXPECT_EQ(json2.size(), 20);

    // Results should be identical
    EXPECT_EQ(json1.dump(), json2.dump());

    // Cached batch should be faster (at least as fast, typically faster)
    // Note: With read-write locks, cache hits may have overhead for updating LRU list
    EXPECT_LE(second_batch_time, first_batch_time * 2.0);
}

// Test batch lookup with mixed valid and invalid IPs
TEST_F(HTTPServerTest, BatchLookupMixedIPs) {
    httplib::Client client("127.0.0.1", test_port);

    nlohmann::json batch_request;
    batch_request["ips"] = {
        "8.8.8.8",          // Valid
        "1.1.1.1",          // Valid
        "999.999.999.999",  // Invalid
        "invalid.ip",       // Invalid
        "9.9.9.9"           // Valid
    };

    auto result = client.Post("/lookup", batch_request.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 5);

    // Check valid IPs
    EXPECT_TRUE(json[0].contains("found"));
    EXPECT_TRUE(json[1].contains("found"));
    EXPECT_TRUE(json[4].contains("found"));

    // Check invalid IPs
    EXPECT_TRUE(json[2].contains("error"));
    EXPECT_TRUE(json[3].contains("error"));
}

// Test concurrent batch requests
TEST_F(HTTPServerTest, ConcurrentBatchRequests) {
    httplib::Client client("127.0.0.1", test_port);

    std::vector<std::future<httplib::Result>> futures;

    // Launch 5 concurrent batch requests
    for (int i = 0; i < 5; ++i) {
        nlohmann::json batch_request;
        batch_request["ips"] = {"8.8.8." + std::to_string(i % 255),
                                "1.1.1." + std::to_string(i % 255),
                                "9.9.9." + std::to_string(i % 255)};

        futures.push_back(std::async(std::launch::async, [&client, batch_request]() {
            return client.Post("/lookup", batch_request.dump(), "application/json");
        }));
    }

    // Wait for all results
    for (auto& future : futures) {
        auto result = future.get();
        ASSERT_TRUE(result);
        EXPECT_EQ(result->status, 200);

        auto json = nlohmann::json::parse(result->body);
        EXPECT_TRUE(json.is_array());
        EXPECT_EQ(json.size(), 3);
    }
}

// Test batch lookup with duplicate IPs
TEST_F(HTTPServerTest, BatchLookupWithDuplicates) {
    httplib::Client client("127.0.0.1", test_port);

    nlohmann::json batch_request;
    batch_request["ips"] = {
        "8.8.8.8",
        "8.8.8.8",  // Duplicate
        "1.1.1.1",
        "8.8.8.8",  // Duplicate again
        "1.1.1.1"   // Duplicate
    };

    auto result = client.Post("/lookup", batch_request.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 5);

    // All results should be valid
    for (const auto& item : json) {
        EXPECT_TRUE(item.contains("ip"));
        EXPECT_TRUE(item.contains("found"));
    }
}

// Test metrics endpoint accuracy after batch lookup
TEST_F(HTTPServerTest, MetricsAfterBatchLookup) {
    httplib::Client client("127.0.0.1", test_port);

    // Get initial metrics from health endpoint
    auto initial_result = client.Get("/health");
    ASSERT_TRUE(initial_result);
    EXPECT_EQ(initial_result->status, 200);

    auto initial_json         = nlohmann::json::parse(initial_result->body);
    uint64_t initial_requests = initial_json["metrics"]["total_requests"];

    // Perform batch lookup with 10 IPs
    nlohmann::json batch_request;
    for (int i = 0; i < 10; ++i) {
        batch_request["ips"].push_back("8.8.8." + std::to_string(i % 255));
    }

    auto batch_result = client.Post("/lookup", batch_request.dump(), "application/json");
    ASSERT_TRUE(batch_result);
    EXPECT_EQ(batch_result->status, 200);

    // Get updated metrics from health endpoint
    auto updated_result = client.Get("/health");
    ASSERT_TRUE(updated_result);
    EXPECT_EQ(updated_result->status, 200);

    auto updated_json         = nlohmann::json::parse(updated_result->body);
    uint64_t updated_requests = updated_json["metrics"]["total_requests"];

    // Request count should increase by 10
    EXPECT_EQ(updated_requests, initial_requests + 10);
}

// Test health endpoint with cache statistics
TEST_F(HTTPServerTest, HealthEndpointWithCacheStats) {
    httplib::Client client("127.0.0.1", test_port);

    // Perform some lookups to populate cache
    client.Get("/lookup?ip=8.8.8.8");
    client.Get("/lookup?ip=1.1.1.1");
    client.Get("/lookup?ip=8.8.8.8");  // Cache hit

    auto result = client.Get("/health");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.contains("cache"));
    EXPECT_TRUE(json["cache"].contains("hits"));
    EXPECT_TRUE(json["cache"].contains("misses"));
    EXPECT_TRUE(json["cache"].contains("hit_rate_percent"));

    // Cache hit rate should be positive
    EXPECT_GT(json["cache"]["hit_rate_percent"], 0.0);
}

// Test batch lookup response ordering
TEST_F(HTTPServerTest, BatchLookupResponseOrdering) {
    httplib::Client client("127.0.0.1", test_port);

    nlohmann::json batch_request;
    std::vector<std::string> test_ips = {"8.8.8.8", "1.1.1.1", "9.9.9.9", "208.67.222.222",
                                         "208.67.220.220"};
    batch_request["ips"]              = test_ips;

    auto result = client.Post("/lookup", batch_request.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 5);

    // Verify response order matches request order
    for (size_t i = 0; i < test_ips.size(); ++i) {
        EXPECT_EQ(json[i]["ip"], test_ips[i]);
    }
}

// Test batch lookup with empty array
TEST_F(HTTPServerTest, BatchLookupEmptyArray) {
    httplib::Client client("127.0.0.1", test_port);

    nlohmann::json batch_request;
    batch_request["ips"] = nlohmann::json::array();

    auto result = client.Post("/lookup", batch_request.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 0);
}

// Test batch lookup with single IP
TEST_F(HTTPServerTest, BatchLookupSingleIP) {
    httplib::Client client("127.0.0.1", test_port);

    nlohmann::json batch_request;
    batch_request["ips"] = {"8.8.8.8"};

    auto result = client.Post("/lookup", batch_request.dump(), "application/json");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 1);
    EXPECT_EQ(json[0]["ip"], "8.8.8.8");
}