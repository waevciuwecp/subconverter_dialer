#include <doctest/doctest.h>

#include <string>

#include "utils/string.h"
#include "utils/urlencode.h"

TEST_CASE("urlEncode and urlDecode basics")
{
    CHECK(urlEncode("a b+c/?") == "a%20b%2Bc%2F%3F");
    CHECK(urlDecode("a+b%2Bc%2F%3F") == "a b+c/?");
}

TEST_CASE("joinArguments emits URL encoded values")
{
    string_multimap args;
    args.emplace("name", "John Doe");
    args.emplace("token", "a+b");
    const std::string query = joinArguments(args);

    CHECK(query.find("name=John%20Doe") != std::string::npos);
    CHECK(query.find("token=a%2Bb") != std::string::npos);
}
