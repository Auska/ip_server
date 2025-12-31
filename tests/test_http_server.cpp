#include <gtest/gtest.h>
#include "http_server.h"
#include "database.h"
#include <thread>
#include <chrono>
#include <httplib.h>
#include <future>
#include <vector>

using namespace ip_server;

class HTTPServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get the project root directory
        std::filesystem::path project_root = std::filesystem::current_path().parent_path();

        city_db_path = project_root / "db" / "GeoLite2-City.mmdb";
        asn_db_path = project_root / "db" / "GeoLite2-ASN.mmdb";

        // Skip tests if database files don't exist
        if (!std::filesystem::exists(city_db_path) || !std::filesystem::exists(asn_db_path)) {
            GTEST_SKIP() << "Database files not found. Skipping HTTP server tests.";
        }

        // Initialize service
        service = std::make_unique<IPGeoService>(city_db_path.string(), asn_db_path.string());

        // Start server on a random port
        test_port = 18080;
        server = std::make_unique<IPGeoHTTPServer>("127.0.0.1", test_port);

        server->set_lookup_handler([this](const std::string& ip) {
            return service->lookup(ip);
        });

        // Start server in background thread
        server_thread = std::thread([this]() {
            server->start();
        });

        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        if (server) {
            server->stop();
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
    EXPECT_EQ(result->status, 400);

    auto json = nlohmann::json::parse(result->body);
    EXPECT_TRUE(json.contains("error"));
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
    httplib::Headers headers = {
        {"Origin", "http://example.com"}
    };

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
        futures.push_back(std::async(std::launch::async, [&client]() {
            return client.Get("/lookup?ip=8.8.8.8");
        }));
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