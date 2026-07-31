#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "rate_limiter.h"

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
    std::string ip2    = "192.168.1.3";

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
    std::string ip2    = "192.168.1.3";

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
    std::string ip    = "192.168.1.1";

    // No requests should be allowed
    EXPECT_FALSE(zero_limiter->is_allowed(ip));
    EXPECT_EQ(zero_limiter->get_remaining(ip), 0);
}

TEST_F(RateLimiterTest, LargeLimit) {
    // Create a rate limiter with a large limit
    auto large_limiter = std::make_unique<RateLimiter>(1000, std::chrono::seconds(60));
    std::string ip     = "192.168.1.1";

    // Make 100 requests
    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(large_limiter->is_allowed(ip));
    }

    // Should still have 900 remaining
    EXPECT_EQ(large_limiter->get_remaining(ip), 900);
}

TEST_F(RateLimiterTest, ConcurrentRequests) {
    std::string ip                = "192.168.1.1";
    const int num_threads         = 10;
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
    std::string ip     = "192.168.1.1";

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
TEST_F(RateLimiterTest, MemoryStats) {
    std::string ip1 = "192.168.1.1";
    std::string ip2 = "192.168.1.2";

    // Make some requests
    for (int i = 0; i < 3; i++) {
        limiter->is_allowed(ip1);
    }

    for (int i = 0; i < 2; i++) {
        limiter->is_allowed(ip2);
    }

    // Get memory stats
    auto stats = limiter->get_memory_stats();

    EXPECT_EQ(stats.ip_record_count_, 2);
    EXPECT_EQ(stats.total_timestamps_, 5);
    EXPECT_EQ(stats.total_requests_, 5);
    EXPECT_GT(stats.estimated_memory_bytes_, 0);
    EXPECT_EQ(stats.total_rate_limited_, 0);
}

TEST_F(RateLimiterTest, MemoryStatsWithRateLimiting) {
    std::string ip = "192.168.1.1";

    // Make 5 requests (all allowed)
    for (int i = 0; i < 5; i++) {
        limiter->is_allowed(ip);
    }

    // Make 2 more requests (both rate limited)
    limiter->is_allowed(ip);
    limiter->is_allowed(ip);

    auto stats = limiter->get_memory_stats();

    EXPECT_EQ(stats.total_requests_, 7);
    EXPECT_EQ(stats.total_rate_limited_, 2);
}

TEST_F(RateLimiterTest, CleanupRemovesIdleRecords) {
    auto short_limiter = std::make_unique<RateLimiter>(5, std::chrono::seconds(2));
    std::string ip1    = "192.168.1.1";
    std::string ip2    = "192.168.1.2";

    // Make requests from both IPs
    short_limiter->is_allowed(ip1);
    short_limiter->is_allowed(ip2);

    EXPECT_EQ(short_limiter->get_memory_stats().ip_record_count_, 2);

    // Wait for window to expire
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Cleanup should remove idle records
    short_limiter->cleanup();

    EXPECT_EQ(short_limiter->get_memory_stats().ip_record_count_, 0);
}

TEST_F(RateLimiterTest, MaxIPRecordsLimit) {
    // Create limiter with max 3 IP records
    auto small_limiter = std::make_unique<RateLimiter>(5, std::chrono::seconds(60), 3);

    // Make requests from 5 different IPs
    for (int i = 1; i <= 5; i++) {
        std::string ip = "192.168.1." + std::to_string(i);
        small_limiter->is_allowed(ip);
    }

    // Should only have 3 IP records (LRU eviction)
    auto stats = small_limiter->get_memory_stats();
    EXPECT_LE(stats.ip_record_count_, 3);
}

TEST_F(RateLimiterTest, LRUEviction) {
    // Create limiter with max 3 IP records
    auto small_limiter = std::make_unique<RateLimiter>(5, std::chrono::seconds(60), 3);

    std::string ip1 = "192.168.1.1";
    std::string ip2 = "192.168.1.2";
    std::string ip3 = "192.168.1.3";
    std::string ip4 = "192.168.1.4";

    // Add 3 IPs
    small_limiter->is_allowed(ip1);
    small_limiter->is_allowed(ip2);
    small_limiter->is_allowed(ip3);

    // Access ip1 to make it recently used
    small_limiter->is_allowed(ip1);

    // Add 4th IP - should evict ip2 (least recently used)
    small_limiter->is_allowed(ip4);

    auto stats = small_limiter->get_memory_stats();
    EXPECT_EQ(stats.ip_record_count_, 3);

    // ip2 should be evicted, so it should have full quota
    EXPECT_EQ(small_limiter->get_remaining(ip2), 5);

    // ip1 should still be in cache
    EXPECT_LT(small_limiter->get_remaining(ip1), 5);
}

TEST_F(RateLimiterTest, ResetStats) {
    std::string ip = "192.168.1.1";

    // Make some requests
    for (int i = 0; i < 3; i++) {
        limiter->is_allowed(ip);
    }

    // Make one rate limited request
    for (int i = 0; i < 6; i++) {
        limiter->is_allowed(ip);
    }

    auto stats_before = limiter->get_memory_stats();
    EXPECT_GT(stats_before.total_requests_, 0);
    EXPECT_GT(stats_before.total_rate_limited_, 0);

    // Reset stats
    limiter->reset_stats();

    auto stats_after = limiter->get_memory_stats();
    EXPECT_EQ(stats_after.total_requests_, 0);
    EXPECT_EQ(stats_after.total_rate_limited_, 0);
}

TEST_F(RateLimiterTest, GetRemainingForNonExistentIP) {
    std::string ip = "192.168.1.999";

    // IP that hasn't made any requests should have full quota
    EXPECT_EQ(limiter->get_remaining(ip), 5);
}

TEST_F(RateLimiterTest, MemoryEstimation) {
    // Create limiter with many IPs
    auto large_limiter = std::make_unique<RateLimiter>(10, std::chrono::seconds(60), 1000);

    // Add 100 IPs with 5 requests each
    for (int i = 0; i < 100; i++) {
        std::string ip = "192.168.1." + std::to_string(i);
        for (int j = 0; j < 5; j++) {
            large_limiter->is_allowed(ip);
        }
    }

    auto stats = large_limiter->get_memory_stats();

    EXPECT_EQ(stats.ip_record_count_, 100);
    EXPECT_EQ(stats.total_timestamps_, 500);
    EXPECT_GT(stats.estimated_memory_bytes_, 0);

    // Memory should be reasonable (less than 1 MB for 100 IPs)
    EXPECT_LT(stats.estimated_memory_bytes_, 1024 * 1024);
}

// ─── Edge-case tests ──────────────────────────────────────────────

TEST_F(RateLimiterTest, ZeroLimitDeniesAll) {
    auto zero_limiter = std::make_unique<RateLimiter>(0, std::chrono::seconds(60));
    // With max_requests=0, every request should be denied.
    EXPECT_FALSE(zero_limiter->is_allowed("10.0.0.1"));
    EXPECT_FALSE(zero_limiter->is_allowed("10.0.0.2"));

    // get_remaining should return 0.
    EXPECT_EQ(zero_limiter->get_remaining("10.0.0.1"), 0);
    EXPECT_EQ(zero_limiter->get_remaining("10.0.0.2"), 0);
}

TEST_F(RateLimiterTest, SingleRequestLimit) {
    auto single_limiter = std::make_unique<RateLimiter>(1, std::chrono::seconds(60));
    std::string ip      = "10.0.0.1";

    EXPECT_TRUE(single_limiter->is_allowed(ip));
    EXPECT_FALSE(single_limiter->is_allowed(ip));
    EXPECT_EQ(single_limiter->get_remaining(ip), 0);
}

TEST_F(RateLimiterTest, MaxIPRecordsLimitOne) {
    auto limiter_one = std::make_unique<RateLimiter>(5, std::chrono::seconds(60), 1);

    // Only one IP record allowed — first IP works, second should evict the first
    EXPECT_TRUE(limiter_one->is_allowed("10.0.0.1"));
    EXPECT_TRUE(limiter_one->is_allowed("10.0.0.2"));

    // First IP record should have been evicted — fresh quota
    EXPECT_EQ(limiter_one->get_remaining("10.0.0.1"), 5);
}

TEST_F(RateLimiterTest, EmptyIPString) {
    // Empty IP string should not crash
    EXPECT_TRUE(limiter->is_allowed(""));
    EXPECT_TRUE(limiter->is_allowed(""));
}

TEST_F(RateLimiterTest, CleanupOnEmptyRecords) {
    auto cleanup_limiter = std::make_unique<RateLimiter>(5, std::chrono::seconds(60), 1000);

    // Add and then immediately clean — no crash
    EXPECT_TRUE(cleanup_limiter->is_allowed("10.0.0.1"));
    EXPECT_NO_THROW(cleanup_limiter->cleanup());
    EXPECT_NO_THROW(cleanup_limiter->cleanup());
}

TEST_F(RateLimiterTest, ResetStatsThenContinue) {
    std::string ip = "10.0.0.1";

    EXPECT_TRUE(limiter->is_allowed(ip));
    limiter->reset_stats();

    // After reset, the IP record still exists — same quota as before
    EXPECT_TRUE(limiter->is_allowed(ip));
    EXPECT_EQ(limiter->get_remaining(ip), 3);
}

TEST_F(RateLimiterTest, HighContentionDifferentIPs) {
    auto contention_limiter = std::make_unique<RateLimiter>(100, std::chrono::seconds(60), 10000);
    std::atomic<int> allowed{0};
    std::atomic<int> denied{0};

    auto worker = [&](int id) {
        for (int i = 0; i < 20; i++) {
            std::string ip = "10.0.0." + std::to_string(id);
            if (contention_limiter->is_allowed(ip))
                allowed++;
            else
                denied++;
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < 20; t++) {
        threads.emplace_back(worker, t);
    }
    for (auto& t : threads) t.join();

    // Each of the 20 IPs can make 100 requests, so all 400 should be allowed
    EXPECT_EQ(allowed, 400);
    EXPECT_EQ(denied, 0);
}

TEST_F(RateLimiterTest, RateLimitedIPGetRemaining) {
    std::string ip = "10.0.0.1";

    // Exhaust all 5 requests
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(limiter->is_allowed(ip));
    }
    // 6th is denied
    EXPECT_FALSE(limiter->is_allowed(ip));
    EXPECT_EQ(limiter->get_remaining(ip), 0);

    // Non-existent IP gets full quota
    EXPECT_EQ(limiter->get_remaining("10.0.0.99"), 5);
}
