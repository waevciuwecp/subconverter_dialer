#include <doctest/doctest.h>

#include <string>

#include "generator/config/ruleconvert.h"
#include "helpers/test_helpers.h"

TEST_CASE("ruleconvert converts QuanX types into common format")
{
    const std::string input = "host,example.com,Proxy\nip6-cidr,2001:db8::/32,Proxy\n";
    const std::string out = convertRuleset(input, RULESET_QUANX);
    CHECK(containsText(out, "DOMAIN,example.com"));
    CHECK(containsText(out, "IP-CIDR6,2001:db8::/32"));
}

TEST_CASE("ruleconvert converts Clash domain payload entries")
{
    const std::string input =
        "payload:\n"
        "  - '.example.com'\n"
        "  - '+.keyword.*'\n"
        "  - '1.2.3.0/24'\n";

    const std::string out = convertRuleset(input, RULESET_CLASH_DOMAIN);
    CHECK(containsText(out, "DOMAIN-SUFFIX,example.com"));
    CHECK(containsText(out, "DOMAIN-KEYWORD,keyword"));
    CHECK(containsText(out, "IP-CIDR,1.2.3.0/24"));
}

TEST_CASE("ruleconvert keeps classical Clash payload shape")
{
    const std::string input =
        "payload:\n"
        "  - 'DOMAIN,example.com'\n";
    const std::string out = convertRuleset(input, RULESET_CLASH_CLASSICAL);
    CHECK(containsText(out, "DOMAIN,example.com"));
}
