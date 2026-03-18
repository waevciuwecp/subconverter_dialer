#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "utils/regexp.h"

TEST_CASE("regexp find/match/replace")
{
    CHECK(regValid("^abc$"));
    CHECK(regMatch("abc", "^abc$"));
    CHECK(regFind("prefix-abc-suffix", "abc"));
    CHECK(regReplace("a1b2c3", "\\d", "x") == "axbxcx");
    CHECK(regTrim("  abc  ") == "abc  ");
}

TEST_CASE("regexp captures")
{
    std::string full;
    std::string first;
    std::string second;
    REQUIRE(regGetMatch("name-42", "(\\w+)-(\\d+)", 3, &full, &first, &second) == 0);
    CHECK(full == "name-42");
    CHECK(first == "name");
    CHECK(second == "42");

    const std::vector<std::string> matches = regGetAllMatch("a1b22", "(\\d+)", true);
    REQUIRE(matches.size() == 2);
    CHECK(matches[0] == "1");
    CHECK(matches[1] == "22");
}
