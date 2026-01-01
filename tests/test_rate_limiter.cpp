#include <gtest/gtest.h>
#include "rate_limiter.h"
#include <thread>
#include <chrono>

using namespace ip_server;

class RateLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a rate limiter with 5 requests per minute
        limiter = std::make_unique<RateLimiter>(5, std::chrono::seconds(60));
    }

    std::unique_ptr<RateLimiter> limiter;
};

TEST_F(RateLimiterTest, BasicRateLimiting) {
    std::string ip = "192.168.1.1";

    // First 5 requests should be allowed
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(limiter->is_allowed(ip)) << "Request " << i << " should be allowed";
    }

    // 6th request should be denied
    EXPECT_FALSE(limiter->is_allowed(ip)) << "6th request should be denied";
}

TEST_F(RateLimiterTest, DifferentIPsIndependent) {
    std::string ip1 = "192.168.1.1";
    std::string ip2 = "192.168.1.2";

    // Make 5 requests from IP1
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(limiter->is_allowed(ip1));
    }

    // IP1 should be rate limited
    EXPECT_FALSE(limiter->is_allowed(ip1));

    // IP2 should still be allowed (independent rate limiting)
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(limiter->is_allowed(ip2)) << "IP2 request " << i << " should be allowed";
    }

    // IP2 should also be rate limited after 5 requests
    EXPECT_FALSE(limiter->is_allowed(ip2));
}

TEST_F(RateLimiterTest, TimeWindowExpiration) {
    std::string ip = "192.168.1.1";

    // Make 5 requests (fill the limit)
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(limiter->is_allowed(ip));
    }

    // Should be rate limited
    EXPECT_FALSE(limiter->is_allowed(ip));

    // Wait for the time window to expire (60 seconds)
    // For testing purposes, we'll use a shorter window
    auto short_limiter = std::make_unique<RateLimiter>(3, std::chrono::seconds(1));
    std::string ip2 = "192.168.1.3";

    // Make 3 requests
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(short_limiter->is_allowed(ip2));
    }

    // Should be rate limited
    EXPECT_FALSE(short_limiter->is_allowed(ip2));

    // Wait for the window to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Should be allowed again after window expires
    EXPECT_TRUE(short_limiter->is_allowed(ip2));
}

TEST_F(RateLimiterTest, GetRemainingRequests) {
    std::string ip = "192.168.1.1";

    // Initially should have 5 remaining
    EXPECT_EQ(limiter->get_remaining(ip), 5);

    // After 1 request, should have 4 remaining
    limiter->is_allowed(ip);
    EXPECT_EQ(limiter->get_remaining(ip), 4);

    // After 5 requests, should have 0 remaining
    for (int i = 1; i < 5; i++) {
        limiter->is_allowed(ip);
    }
    EXPECT_EQ(limiter->get_remaining(ip), 0);

    // For a new IP, should have 5 remaining
    std::string ip2 = "192.168.1.2";
    EXPECT_EQ(limiter->get_remaining(ip2), 5);
}

TEST_F(RateLimiterTest, CleanupOldEntries) {
    std::string ip = "192.168.1.1";

    // Make some requests
    for (int i = 0; i < 3; i++) {
        limiter->is_allowed(ip);
    }

    // Wait for entries to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Create a short-lived limiter for testing cleanup
    auto short_limiter = std::make_unique<RateLimiter>(3, std::chrono::seconds(1));
    std::string ip2 = "192.168.1.3";

    // Make requests
    for (int i = 0; i < 3; i++) {
        short_limiter->is_allowed(ip2);
    }

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Cleanup should remove old entries
    short_limiter->cleanup();

    // After cleanup and expiration, should be allowed again
    EXPECT_TRUE(short_limiter->is_allowed(ip2));
}

TEST_F(RateLimiterTest, ZeroLimit) {
    // Create a rate limiter with 0 requests allowed
    auto zero_limiter = std::make_unique<RateLimiter>(0, std::chrono::seconds(60));
    std::string ip = "192.168.1.1";

    // No requests should be allowed
    EXPECT_FALSE(zero_limiter->is_allowed(ip));
    EXPECT_EQ(zero_limiter->get_remaining(ip), 0);
}

TEST_F(RateLimiterTest, LargeLimit) {
    // Create a rate limiter with a large limit
    auto large_limiter = std::make_unique<RateLimiter>(1000, std::chrono::seconds(60));
    std::string ip = "192.168.1.1";

    // Make 100 requests
    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(large_limiter->is_allowed(ip));
    }

    // Should still have 900 remaining
    EXPECT_EQ(large_limiter->get_remaining(ip), 900);
}

TEST_F(RateLimiterTest, ConcurrentRequests) {
    std::string ip = "192.168.1.1";
    const int num_threads = 10;
    const int requests_per_thread = 10;

    std::vector<std::thread> threads;
    std::atomic<int> allowed_count(0);
    std::atomic<int> denied_count(0);

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &ip, &allowed_count, &denied_count, requests_per_thread]() {
            for (int i = 0; i < requests_per_thread; i++) {
                if (limiter->is_allowed(ip)) {
                    allowed_count++;
                } else {
                    denied_count++;
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Total requests should be num_threads * requests_per_thread
    // But only 5 should be allowed (the limit)
    EXPECT_EQ(allowed_count.load(), 5);
    EXPECT_EQ(denied_count.load(), num_threads * requests_per_thread - 5);
}

TEST_F(RateLimiterTest, IPv6Address) {
    std::string ipv6 = "2001:0db8:85a3:0000:0000:8a2e:0370:7334";

    // First 5 requests should be allowed
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(limiter->is_allowed(ipv6));
    }

    // 6th request should be denied
    EXPECT_FALSE(limiter->is_allowed(ipv6));
}

TEST_F(RateLimiterTest, SlidingWindowBehavior) {
    auto short_limiter = std::make_unique<RateLimiter>(3, std::chrono::seconds(2));
    std::string ip = "192.168.1.1";

    // Make 3 requests at t=0
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(short_limiter->is_allowed(ip));
    }

    // Should be rate limited
    EXPECT_FALSE(short_limiter->is_allowed(ip));

    // Wait 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Still rate limited (window is 2 seconds)
    EXPECT_FALSE(short_limiter->is_allowed(ip));

    // Wait another 1.5 seconds (total 2.5 seconds)
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    // Should be allowed now (first request has expired from window)
    EXPECT_TRUE(short_limiter->is_allowed(ip));
}