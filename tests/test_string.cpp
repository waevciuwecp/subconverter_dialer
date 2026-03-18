#include <doctest/doctest.h>

#include <string_view>
#include <vector>

#include "utils/string.h"

TEST_CASE("string split/join basics")
{
    const string_array tokens = split("a,b,c", ",");
    REQUIRE(tokens.size() == 3);
    CHECK(tokens[0] == "a");
    CHECK(tokens[1] == "b");
    CHECK(tokens[2] == "c");

    const std::vector<std::string_view> views = split(std::string_view("x|y|z"), '|');
    REQUIRE(views.size() == 3);
    CHECK(views[0] == "x");
    CHECK(views[2] == "z");

    CHECK(join(tokens, "-") == "a-b-c");
}

TEST_CASE("string trimming and replacement")
{
    CHECK(trim("  abc  ") == "abc");
    CHECK(trimWhitespace(" \t abc \n", true, true) == "abc");
    CHECK(trimQuote("\"quoted\"") == "quoted");
    CHECK(replaceAllDistinct("a--b--c", "--", ":") == "a:b:c");
}

TEST_CASE("string query helpers")
{
    CHECK(getUrlArg("https://x.test/path?a=1&b=2", "a") == "1");
    CHECK(getUrlArg("a=1&b=2&a=3", "a") == "3");
    CHECK(getUrlArg("k=v", "missing").empty());

    string_multimap args;
    args.emplace("token", "abc");
    CHECK(getUrlArg(args, "token") == "abc");
}

TEST_CASE("string parseCommaKeyValue supports escaped separator")
{
    string_pair_array parsed;
    REQUIRE(parseCommaKeyValue("k1=v1,k2=v\\,2,solo", ",", parsed) == 0);
    REQUIRE(parsed.size() == 3);
    CHECK(parsed[0].first == "k1");
    CHECK(parsed[0].second == "v1");
    CHECK(parsed[1].first == "k2");
    CHECK(parsed[1].second == "v,2");
    CHECK(parsed[2].first == "{NONAME}");
    CHECK(parsed[2].second == "solo");
}

TEST_CASE("string UTF-8 helpers")
{
    std::string with_bom = "\xEF\xBB\xBFhello";
    removeUTF8BOM(with_bom);
    CHECK(with_bom == "hello");
    CHECK(isStrUTF8("hello"));
}
