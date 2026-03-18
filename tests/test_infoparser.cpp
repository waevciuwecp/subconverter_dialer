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

TEST_CASE("infoparser streamToInt supports binary units")
{
    CHECK(streamToInt("512MB") == 536870912ULL);
    CHECK(streamToInt("1.5GB") == 1610612736ULL);
}
