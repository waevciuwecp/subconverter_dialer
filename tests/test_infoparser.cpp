#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "config/regmatch.h"
#include "helpers/test_helpers.h"
#include "parser/infoparser.h"
#include "utils/base64/base64.h"

TEST_CASE("infoparser extracts Subscription-UserInfo from header")
{
    std::string result;
    const bool ok = getSubInfoFromHeader("Subscription-UserInfo: upload=1; download=2; total=3;\n", result);
    REQUIRE(ok);
    CHECK(result == "upload=1; download=2; total=3;");
}

TEST_CASE("infoparser returns false when Subscription-UserInfo header is absent")
{
    std::string result;
    CHECK_FALSE(getSubInfoFromHeader("Date: x\nServer: y\n", result));
    CHECK(result.empty());
}

TEST_CASE("infoparser extracts stream and time info from nodes")
{
    std::vector<Proxy> nodes = {makeProxyWithRemark("node A")};
    const RegexMatchConfigs stream_rules = {RegexMatchConfig{"^.*$", "total=10GB&used=2GB", ""}};
    const RegexMatchConfigs time_rules = {RegexMatchConfig{"^.*$", "left=1d", ""}};

    std::string result;
    REQUIRE(getSubInfoFromNodes(nodes, stream_rules, time_rules, result));
    CHECK(containsText(result, "download=2147483648;"));
    CHECK(containsText(result, "total=10737418240;"));
    CHECK(containsText(result, "expire="));
}

TEST_CASE("infoparser handles percent stream formulas and invalid date format")
{
    std::vector<Proxy> nodes = {makeProxyWithRemark("node B")};
    std::string result;

    {
        const RegexMatchConfigs stream_rules = {RegexMatchConfig{"^.*$", "total=80%&used=2GB", ""}};
        const RegexMatchConfigs time_rules = {RegexMatchConfig{"^.*$", "2026:03:18:10:20:30", ""}};
        REQUIRE(getSubInfoFromNodes(nodes, stream_rules, time_rules, result));
        CHECK(containsText(result, "download=2147483648;"));
        CHECK(containsText(result, "total=10737418240;"));
        CHECK(containsText(result, "expire="));
    }

    {
        const RegexMatchConfigs stream_rules = {RegexMatchConfig{"^.*$", "total=25%&left=1GB", ""}};
        const RegexMatchConfigs time_rules = {RegexMatchConfig{"^.*$", "left=1d", ""}};
        REQUIRE(getSubInfoFromNodes(nodes, stream_rules, time_rules, result));
        CHECK(containsText(result, "download=3221225472;"));
        CHECK(containsText(result, "total=4294967296;"));
        CHECK(containsText(result, "expire="));
    }

    {
        const RegexMatchConfigs stream_rules = {RegexMatchConfig{"^.*$", "total=5GB&used=1GB", ""}};
        const RegexMatchConfigs time_rules = {RegexMatchConfig{"^.*$", "bad-date", ""}};
        REQUIRE(getSubInfoFromNodes(nodes, stream_rules, time_rules, result));
        CHECK(containsText(result, "download=1073741824;"));
        CHECK(containsText(result, "total=5368709120;"));
        CHECK_FALSE(containsText(result, "expire="));
    }
}

TEST_CASE("infoparser handles left greater than total and unmatched rules")
{
    std::vector<Proxy> nodes = {makeProxyWithRemark("node C")};
    std::string result;

    const RegexMatchConfigs stream_rules = {RegexMatchConfig{"^.*$", "total=1GB&left=2GB", ""}};
    const RegexMatchConfigs no_time_rules = {};
    REQUIRE(getSubInfoFromNodes(nodes, stream_rules, no_time_rules, result));
    CHECK(result == "upload=0; download=1073741824; total=1073741824;");

    const RegexMatchConfigs miss_rules = {RegexMatchConfig{"^unmatched$", "total=1GB&used=1GB", ""}};
    result.clear();
    CHECK_FALSE(getSubInfoFromNodes(nodes, miss_rules, miss_rules, result));
}

TEST_CASE("infoparser decodes SSD payload")
{
    const std::string json = R"({"traffic_used":"1","traffic_total":"5","expiry":"2025-01-02 03:04:05"})";
    const std::string sub = "ssd://" + urlSafeBase64Encode(json);

    std::string result;
    REQUIRE(getSubInfoFromSSD(sub, result));
    CHECK(containsText(result, "download=1073741824;"));
    CHECK(containsText(result, "total=5368709120;"));
    CHECK(containsText(result, "expire="));
}

TEST_CASE("infoparser rejects malformed SSD payload")
{
    std::string result;
    const std::string missing_total_json = R"({"traffic_used":"1.5","expiry":"2026-03-18 10:20:30"})";
    const std::string invalid_json = "{broken json";
    CHECK_FALSE(getSubInfoFromSSD("ssd://" + urlSafeBase64Encode(missing_total_json), result));
    CHECK_FALSE(getSubInfoFromSSD("ssd://" + urlSafeBase64Encode(invalid_json), result));
}

TEST_CASE("infoparser streamToInt supports binary units")
{
    CHECK(streamToInt("") == 0ULL);
    CHECK(streamToInt("512MB") == 536870912ULL);
    CHECK(streamToInt("1.5GB") == 1610612736ULL);
    CHECK(streamToInt("3TB") == 3298534883328ULL);
}
