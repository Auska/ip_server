#include <gtest/gtest.h>
#include "types.h"
#include <nlohmann/json.hpp>

using namespace ip_server;

class TypesTest : public ::testing::Test {
protected:
    IPGeoInfo create_test_info() {
        IPGeoInfo info;
        info.ip = "8.8.8.8";
        info.found = true;
        info.country = "United States";
        info.country_code = "US";
        info.city = "Mountain View";
        info.continent = "North America";
        info.latitude = 37.4223;
        info.longitude = -122.085;
        info.timezone = "America/Los_Angeles";
        info.as_organization = "Google LLC";
        info.as_number = 15169;
        return info;
    }
};

TEST_F(TypesTest, DefaultValues) {
    IPGeoInfo info;

    EXPECT_TRUE(info.ip.empty());
    EXPECT_FALSE(info.found);
    EXPECT_TRUE(info.error.empty());
    EXPECT_TRUE(info.country.empty());
    EXPECT_TRUE(info.country_code.empty());
    EXPECT_TRUE(info.city.empty());
    EXPECT_TRUE(info.continent.empty());
    EXPECT_EQ(info.latitude, 0.0);
    EXPECT_EQ(info.longitude, 0.0);
    EXPECT_TRUE(info.timezone.empty());
    EXPECT_TRUE(info.as_organization.empty());
    EXPECT_EQ(info.as_number, 0);
}

TEST_F(TypesTest, ToJsonCompleteInfo) {
    IPGeoInfo info = create_test_info();
    auto json = info.to_json();

    EXPECT_EQ(json["ip"], "8.8.8.8");
    EXPECT_TRUE(json["found"]);
    EXPECT_EQ(json["country"], "United States");
    EXPECT_EQ(json["country_code"], "US");
    EXPECT_EQ(json["city"], "Mountain View");
    EXPECT_EQ(json["continent"], "North America");
    EXPECT_NEAR(json["latitude"], 37.4223, 0.0001);
    EXPECT_NEAR(json["longitude"], -122.085, 0.0001);
    EXPECT_EQ(json["timezone"], "America/Los_Angeles");
    EXPECT_EQ(json["as_organization"], "Google LLC");
    EXPECT_EQ(json["as_number"], 15169);
}

TEST_F(TypesTest, ToJsonNotFound) {
    IPGeoInfo info;
    info.ip = "1.2.3.4";
    info.found = false;

    auto json = info.to_json();

    EXPECT_EQ(json["ip"], "1.2.3.4");
    EXPECT_FALSE(json["found"]);
    EXPECT_FALSE(json.contains("country"));
    EXPECT_FALSE(json.contains("city"));
}

TEST_F(TypesTest, ToJsonWithError) {
    IPGeoInfo info;
    info.ip = "invalid.ip";
    info.found = false;
    info.error = "Invalid IP address";

    auto json = info.to_json();

    EXPECT_EQ(json["ip"], "invalid.ip");
    EXPECT_FALSE(json["found"]);
    EXPECT_EQ(json["error"], "Invalid IP address");
    EXPECT_FALSE(json.contains("country"));
    EXPECT_FALSE(json.contains("city"));
}

TEST_F(TypesTest, ToJsonPartialInfo) {
    IPGeoInfo info;
    info.ip = "8.8.8.8";
    info.found = true;
    info.country = "United States";
    info.country_code = "US";

    auto json = info.to_json();

    EXPECT_EQ(json["ip"], "8.8.8.8");
    EXPECT_TRUE(json["found"]);
    EXPECT_EQ(json["country"], "United States");
    EXPECT_EQ(json["country_code"], "US");
    EXPECT_FALSE(json.contains("city"));
    EXPECT_FALSE(json.contains("latitude"));
}

TEST_F(TypesTest, ToJsonWithCoordinates) {
    IPGeoInfo info;
    info.ip = "8.8.8.8";
    info.found = true;
    info.latitude = 40.7128;
    info.longitude = -74.0060;

    auto json = info.to_json();

    EXPECT_NEAR(json["latitude"], 40.7128, 0.0001);
    EXPECT_NEAR(json["longitude"], -74.0060, 0.0001);
}

TEST_F(TypesTest, ToJsonWithASInfo) {
    IPGeoInfo info;
    info.ip = "1.1.1.1";
    info.found = true;
    info.as_organization = "Cloudflare, Inc.";
    info.as_number = 13335;

    auto json = info.to_json();

    EXPECT_EQ(json["as_organization"], "Cloudflare, Inc.");
    EXPECT_EQ(json["as_number"], 13335);
}

TEST_F(TypesTest, ToJsonEmptyStringFieldsNotIncluded) {
    IPGeoInfo info;
    info.ip = "8.8.8.8";
    info.found = true;
    info.country = "United States";
    info.city = "";  // Empty string

    auto json = info.to_json();

    EXPECT_TRUE(json.contains("country"));
    EXPECT_FALSE(json.contains("city"));
}

TEST_F(TypesTest, ToJsonZeroValuesNotIncluded) {
    IPGeoInfo info;
    info.ip = "8.8.8.8";
    info.found = true;
    info.latitude = 37.4223;
    info.longitude = 0.0;  // Zero value
    info.as_number = 0;     // Zero value

    auto json = info.to_json();

    EXPECT_TRUE(json.contains("latitude"));
    EXPECT_FALSE(json.contains("longitude"));
    EXPECT_FALSE(json.contains("as_number"));
}