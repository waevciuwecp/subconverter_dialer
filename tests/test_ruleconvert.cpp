#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "generator/config/ruleconvert.h"
#include "handler/settings.h"
#include "helpers/test_helpers.h"
#include "utils/base64/base64.h"
#include "utils/file.h"

namespace
{
RulesetContent makeRulesetContent(const std::string &group, std::string content, int type = RULESET_SURGE)
{
    RulesetContent rc;
    rc.rule_group = group;
    rc.rule_type = type;
    rc.rule_content = makeReadyFuture(std::move(content));
    return rc;
}

std::string makeTempRuleFilePath()
{
    const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return (std::filesystem::temp_directory_path() / ("subconverter-ruleset-" + suffix + ".list")).string();
}

const rapidjson::Value *findRouteRuleByOutbound(const rapidjson::Document &doc, const std::string &outbound)
{
    if(!doc.HasMember("route") || !doc["route"].HasMember("rules") || !doc["route"]["rules"].IsArray())
        return nullptr;
    const auto &rules = doc["route"]["rules"];
    for(const auto &rule : rules.GetArray())
    {
        if(!rule.IsObject() || !rule.HasMember("outbound") || !rule["outbound"].IsString())
            continue;
        if(outbound == rule["outbound"].GetString())
            return &rule;
    }
    return nullptr;
}

struct ClashModeGuard
{
    bool old_value = global.singBoxAddClashModes;
    ~ClashModeGuard()
    {
        global.singBoxAddClashModes = old_value;
    }
};
} // namespace

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

TEST_CASE("ruleconvert clash string keeps existing rules and appends new rules")
{
    YAML::Node base;
    base["rules"].push_back("DOMAIN,kept.com,KEEP");
    std::vector<RulesetContent> rulesets = {makeRulesetContent("Proxy", "DOMAIN,new.com\n")};

    const std::string out = rulesetToClashStr(base, rulesets, false, true);
    CHECK(containsText(out, "  - DOMAIN,kept.com,KEEP"));
    CHECK(containsText(out, "  - DOMAIN,new.com,Proxy"));
}

TEST_CASE("ruleconvert clash string handles logical and special rule types")
{
    YAML::Node base;
    std::vector<RulesetContent> rulesets = {
        makeRulesetContent("G", "AND,(DOMAIN,a.com),(NETWORK,tcp)\nOR,(DOMAIN,b.com),(NETWORK,udp)\nNOT,(DOMAIN,c.com)\n"),
        makeRulesetContent("Proxy", "SUB-RULE,Apple\nRULE-SET,Netflix\nDOMAIN,ok.com\n"),
        makeRulesetContent("DIRECT", "[]FINAL")
    };

    const std::string out = rulesetToClashStr(base, rulesets, true, true);
    CHECK(containsText(out, "  - AND,(DOMAIN,a.com),(NETWORK,tcp),G"));
    CHECK(containsText(out, "  - OR,(DOMAIN,b.com),(NETWORK,udp),G"));
    CHECK(containsText(out, "  - NOT,(DOMAIN,c.com),G"));
    CHECK(containsText(out, "  - SUB-RULE,Apple"));
    CHECK(containsText(out, "  - RULE-SET,Netflix"));
    CHECK(containsText(out, "  - DOMAIN,ok.com,Proxy"));
    CHECK(containsText(out, "  - MATCH,DIRECT"));
}

TEST_CASE("ruleconvert surge output handles inline and remote rules")
{
    SUBCASE("inline []MATCH rewrites to FINAL for surge")
    {
        INIReader ini;
        std::vector<RulesetContent> rulesets = {makeRulesetContent("DIRECT", "[]MATCH")};
        rulesetToSurge(ini, rulesets, 3, true, "");
        CHECK(containsText(ini.to_string(), "FINAL,DIRECT"));
    }

    SUBCASE("existing local file uses managed remote path for surge v3+")
    {
        const std::string local_path = makeTempRuleFilePath();
        REQUIRE(fileWrite(local_path, "DOMAIN,unit.test\n", true) == 0);

        INIReader ini;
        RulesetContent rc = makeRulesetContent("Proxy", "DOMAIN,ignored.test\n");
        rc.rule_path = local_path;
        rc.rule_path_typed = "https://example.com/list.txt";
        rc.update_interval = 86400;
        std::vector<RulesetContent> rulesets = {rc};
        rulesetToSurge(ini, rulesets, 3, true, "https://srv");

        const std::string encoded = urlSafeBase64Encode(rc.rule_path_typed);
        const std::string expected = "RULE-SET,https://srv/getruleset?type=1&url=" + encoded + ",Proxy,update-interval=86400";
        CHECK(containsText(ini.to_string(), expected));

        std::filesystem::remove(local_path);
    }

    SUBCASE("remote surge link is passed through with update interval")
    {
        INIReader ini;
        RulesetContent rc = makeRulesetContent("Proxy", "");
        rc.rule_path = "https://example.com/list.txt";
        rc.rule_type = RULESET_SURGE;
        rc.update_interval = 86400;
        std::vector<RulesetContent> rulesets = {rc};
        rulesetToSurge(ini, rulesets, 4, true, "");
        CHECK(containsText(ini.to_string(), "RULE-SET,https://example.com/list.txt,Proxy,update-interval=86400"));
    }
}

TEST_CASE("ruleconvert surge quanx branch rewrites ip-cidr6 and strips unsupported option")
{
    const std::string local_path = makeTempRuleFilePath();
    REQUIRE(fileWrite(local_path, "placeholder", true) == 0);

    INIReader ini;
    RulesetContent rc = makeRulesetContent("G", "IP-CIDR6,2001:db8::/32,no-resolve\nIP-CIDR,1.1.1.0/24,other-flag\n");
    rc.rule_path = local_path;
    rc.rule_type = RULESET_SURGE;
    std::vector<RulesetContent> rulesets = {rc};

    rulesetToSurge(ini, rulesets, -1, true, "");
    const std::string output = ini.to_string();
    CHECK(containsText(output, "[filter_local]"));
    CHECK(containsText(output, "IP6-CIDR,2001:db8::/32,G,no-resolve"));
    CHECK(containsText(output, "IP-CIDR,1.1.1.0/24,G"));
    CHECK_FALSE(containsText(output, "other-flag"));

    std::filesystem::remove(local_path);
}

TEST_CASE("ruleconvert sing-box skips deprecated db rules on 1.12+")
{
    ClashModeGuard guard;
    global.singBoxAddClashModes = false;

    rapidjson::Document doc;
    doc.SetObject();
    std::vector<RulesetContent> rulesets = {
        makeRulesetContent("DIRECT", "[]FINAL"),
        makeRulesetContent("Proxy", "GEOIP,CN\nGEOSITE,netflix\nDOMAIN,Example.com\nIP-CIDR6,2001:db8::/32\n")
    };
    rulesetToSingBox(doc, rulesets, true, true, "1.12.0");

    REQUIRE(doc.HasMember("route"));
    REQUIRE(doc["route"].HasMember("final"));
    CHECK(std::string(doc["route"]["final"].GetString()) == "DIRECT");

    bool found_sniff = false;
    bool found_hijack_dns = false;
    for(const auto &rule : doc["route"]["rules"].GetArray())
    {
        if(rule.IsObject() && rule.HasMember("action") && rule["action"].IsString())
        {
            const std::string action = rule["action"].GetString();
            found_sniff = found_sniff || action == "sniff";
            found_hijack_dns = found_hijack_dns || action == "hijack-dns";
        }
    }
    CHECK(found_sniff);
    CHECK(found_hijack_dns);

    const rapidjson::Value *proxy_rule = findRouteRuleByOutbound(doc, "Proxy");
    REQUIRE(proxy_rule != nullptr);
    CHECK(proxy_rule->HasMember("action"));
    CHECK(std::string((*proxy_rule)["action"].GetString()) == "route");
    CHECK(proxy_rule->HasMember("domain"));
    CHECK((*proxy_rule)["domain"].IsArray());
    CHECK(std::string((*proxy_rule)["domain"][0].GetString()) == "example.com");
    CHECK_FALSE(proxy_rule->HasMember("ip_cidr"));
    CHECK_FALSE(proxy_rule->HasMember("geoip"));
    CHECK_FALSE(proxy_rule->HasMember("geosite"));

}

TEST_CASE("ruleconvert sing-box keeps deprecated db rules before 1.12")
{
    ClashModeGuard guard;
    global.singBoxAddClashModes = false;

    rapidjson::Document doc;
    doc.SetObject();
    std::vector<RulesetContent> rulesets = {
        makeRulesetContent("Proxy", "GEOIP,CN\nGEOSITE,google\nDOMAIN,Example.com\n")
    };
    rulesetToSingBox(doc, rulesets, true, false, "1.11.0");

    const rapidjson::Value *proxy_rule = findRouteRuleByOutbound(doc, "Proxy");
    REQUIRE(proxy_rule != nullptr);
    CHECK(proxy_rule->HasMember("geoip"));
    CHECK(proxy_rule->HasMember("geosite"));
    CHECK(proxy_rule->HasMember("domain"));
    CHECK_FALSE(proxy_rule->HasMember("action"));

    bool found_sniff = false;
    for(const auto &rule : doc["route"]["rules"].GetArray())
    {
        if(rule.IsObject() && rule.HasMember("action") && rule["action"].IsString() && std::string(rule["action"].GetString()) == "sniff")
            found_sniff = true;
    }
    CHECK_FALSE(found_sniff);

}
