#include <doctest/doctest.h>

#include <string>

#include "utils/base64/base64.h"

TEST_CASE("base64 roundtrip")
{
    const std::string input = "subconverter-unit-test";
    const std::string encoded = base64Encode(input);
    CHECK(encoded == "c3ViY29udmVydGVyLXVuaXQtdGVzdA==");
    CHECK(base64Decode(encoded) == input);
}

TEST_CASE("base64 urlsafe helpers")
{
    CHECK(urlSafeBase64Apply("+/==") == "-_");
    CHECK(urlSafeBase64Reverse("-_") == "+/");

    const std::string input = "a+b/c==";
    const std::string safe = urlSafeBase64Encode(input);
    CHECK(urlSafeBase64Decode(safe) == input);
}
