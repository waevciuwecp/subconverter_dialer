#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include <rapidjson/document.h>

#include "generator/config/subexport.h"
#include "generator/config/nodemanip.h"
#include "handler/settings.h"
#include "utils/base64/base64.h"

namespace
{
Proxy makeShadowsocksProxy(const std::string &remark, const std::string &group = "UnitGroup")
{
    Proxy node;
    node.Type = ProxyType::Shadowsocks;
    node.Remark = remark;
    node.Group = group;
    node.GroupId = 1;
    node.Hostname = "1.1.1.1";
    node.Port = 8388;
    node.EncryptMethod = "aes-128-gcm";
    node.Password = "password";
    return node;
}

Proxy makeWireGuardProxy(const std::string &remark)
{
    Proxy node;
    node.Type = ProxyType::WireGuard;
    node.Remark = remark;
    node.Group = "WG";
    node.GroupId = 1;
    node.Hostname = "wg.example.com";
    node.Port = 51820;
    node.SelfIP = "10.0.0.2";
    node.PrivateKey = "private-key";
    node.PublicKey = "public-key";
    node.AllowedIPs = "0.0.0.0/0";
    node.Mtu = 1280;
    return node;
}

const rapidjson::Value *findOutboundByTag(const rapidjson::Document &doc, const std::string &tag)
{
    if(!doc.HasMember("outbounds") || !doc["outbounds"].IsArray())
        return nullptr;

    for(const auto &outbound : doc["outbounds"].GetArray())
    {
        if(!outbound.IsObject() || !outbound.HasMember("tag") || !outbound["tag"].IsString())
            continue;
        if(tag == outbound["tag"].GetString())
            return &outbound;
    }
    return nullptr;
}

bool jsonArrayContainsString(const rapidjson::Value &array, const std::string &value)
{
    if(!array.IsArray())
        return false;
    for(const auto &item : array.GetArray())
    {
        if(item.IsString() && value == item.GetString())
            return true;
    }
    return false;
}

std::string makeDataUrl(const std::string &content)
{
    return "data:text/plain;base64," + urlSafeBase64Encode(content);
}

struct SingBoxModesGuard
{
    bool old_value = global.singBoxAddClashModes;

    explicit SingBoxModesGuard(bool replacement)
    {
        global.singBoxAddClashModes = replacement;
    }

    ~SingBoxModesGuard()
    {
        global.singBoxAddClashModes = old_value;
    }
};

} // namespace

TEST_CASE("nodemanip matcher handles GROUP/INSERT/TYPE/PORT/SERVER branches deterministically")
{
    Proxy node;
    node.Type = ProxyType::VMess;
    node.Group = "HK-Edge";
    node.GroupId = 3;
    node.Port = 443;
    node.Hostname = "edge.example.com";
    node.Remark = "HK vmess";

    std::string real_rule;
    REQUIRE(applyMatcher("!!GROUP=^HK-.*$!!HK", real_rule, node));
    CHECK(real_rule == "HK");

    REQUIRE(applyMatcher("!!GROUPID=2-4!!ok", real_rule, node));
    CHECK(real_rule == "ok");

    REQUIRE(applyMatcher("!!INSERT=-3!!ok", real_rule, node));
    CHECK(real_rule == "ok");

    REQUIRE(applyMatcher("!!TYPE=VMESS!!ok", real_rule, node));
    CHECK(real_rule == "ok");

    REQUIRE(applyMatcher("!!PORT=443!!ok", real_rule, node));
    CHECK(real_rule == "ok");

    REQUIRE(applyMatcher("!!SERVER=edge\\.example\\.com!!ok", real_rule, node));
    CHECK(real_rule == "ok");

    CHECK_FALSE(applyMatcher("!!TYPE=SS!!ignored", real_rule, node));
}

TEST_CASE("nodemanip filterNodes applies matcher-driven include/exclude and rewrites ids")
{
    std::vector<Proxy> nodes;

    Proxy a;
    a.Type = ProxyType::VMess;
    a.Remark = "HK-A";
    a.Group = "G";
    a.GroupId = 1;
    a.Port = 443;
    a.Hostname = "a.example.com";
    nodes.push_back(a);

    Proxy b = a;
    b.Remark = "US-B";
    b.Hostname = "b.example.com";
    nodes.push_back(b);

    Proxy c = a;
    c.Type = ProxyType::Shadowsocks;
    c.Remark = "HK-C";
    c.Port = 80;
    c.Hostname = "c.example.com";
    nodes.push_back(c);

    string_array exclude_remarks = {"!!TYPE=VMESS!!US"};
    string_array include_remarks = {"!!PORT=443!!HK"};

    filterNodes(nodes, exclude_remarks, include_remarks, 9);

    REQUIRE(nodes.size() == 1);
    CHECK(nodes[0].Remark == "HK-A");
    CHECK(nodes[0].Id == 0);
    CHECK(nodes[0].GroupId == 9);
}

TEST_CASE("subexport clash resolves provider use list with dedupe and regex filters")
{
    std::vector<Proxy> nodes = {makeShadowsocksProxy("BaseNode")};
    std::vector<RulesetContent> rulesets;

    ProxyGroupConfig group;
    group.Name = "AUTO";
    group.Type = ProxyGroupType::Select;
    group.UsingProvider = {"A"};
    group.ProviderFilterRules = {"^B$", "^A$"};

    extra_settings ext;
    ext.enable_rule_generator = false;
    ext.clash_new_field_name = true;
    ext.clash_proxy_providers = {
        ClashProxyProviderConfig{"A", "http", "https://example.invalid/a.yaml", "a.yaml", 600},
        ClashProxyProviderConfig{"B", "http", "https://example.invalid/b.yaml", "b.yaml", 0},
    };

    const std::string output = proxyToClash(nodes, "{}", rulesets, ProxyGroupConfigs{group}, false, ext);
    YAML::Node root = YAML::Load(output);

    REQUIRE(root["proxy-groups"].IsDefined());
    REQUIRE(root["proxy-groups"].size() == 1);

    const YAML::Node use = root["proxy-groups"][0]["use"];
    REQUIRE(use.IsSequence());
    REQUIRE(use.size() == 2);
    CHECK(use[0].as<std::string>() == "A");
    CHECK(use[1].as<std::string>() == "B");

    REQUIRE(root["proxy-providers"].IsDefined());
    CHECK(root["proxy-providers"]["A"]["interval"].as<int>() == 600);
    CHECK(root["proxy-providers"]["B"]["url"].as<std::string>() == "https://example.invalid/b.yaml");
}

TEST_CASE("subexport sing-box migration rewrites legacy dns and ntp structures for >=1.12.0")
{
    std::vector<Proxy> nodes;
    std::vector<RulesetContent> rulesets;

    extra_settings ext;
    ext.enable_rule_generator = false;
    ext.singbox_version = "1.12.0";

    const std::string base_conf = R"({
  "dns": {
    "fakeip": {
      "inet4_range": "198.18.0.0/15",
      "inet6_range": "fc00::/18"
    },
    "servers": [
      {
        "tag": "dns_resolver",
        "address": "https://dns.example:8443/dns-query",
        "detour": "DIRECT",
        "address_resolver": "bootstrap",
        "address_strategy": "prefer_ipv4"
      },
      {
        "tag": "drop_rcode",
        "address": "rcode://success"
      },
      {
        "tag": "udp_default",
        "address": "8.8.8.8"
      }
    ],
    "rules": [
      {
        "server": "dns_resolver",
        "domain_suffix": ["example.com"]
      },
      {
        "outbound": "DIRECT",
        "server": "dns_resolver"
      },
      {
        "server": "missing",
        "domain": ["drop.test"]
      }
    ]
  },
  "ntp": {
    "enabled": true,
    "detour": "DIRECT"
  }
})";

    const std::string output = proxyToSingBox(nodes, base_conf, rulesets, ProxyGroupConfigs{}, ext);
    rapidjson::Document doc;
    doc.Parse(output.c_str());

    REQUIRE(!doc.HasParseError());
    REQUIRE(doc.HasMember("dns"));
    CHECK_FALSE(doc["dns"].HasMember("fakeip"));

    const auto &servers = doc["dns"]["servers"];
    REQUIRE(servers.IsArray());
    CHECK(servers.Size() == 2);

    const rapidjson::Value *dns_resolver = nullptr;
    for(const auto &server : servers.GetArray())
    {
        if(server.IsObject() && server.HasMember("tag") && server["tag"].IsString() &&
           std::string(server["tag"].GetString()) == "dns_resolver")
        {
            dns_resolver = &server;
            break;
        }
    }
    REQUIRE(dns_resolver != nullptr);
    CHECK(std::string((*dns_resolver)["type"].GetString()) == "https");
    CHECK(std::string((*dns_resolver)["server"].GetString()) == "dns.example");
    CHECK((*dns_resolver)["server_port"].GetInt() == 8443);
    CHECK(std::string((*dns_resolver)["path"].GetString()) == "/dns-query");
    CHECK_FALSE(dns_resolver->HasMember("detour"));
    CHECK(std::string((*dns_resolver)["domain_resolver"].GetString()) == "bootstrap");
    CHECK(std::string((*dns_resolver)["domain_strategy"].GetString()) == "prefer_ipv4");

    const auto &rules = doc["dns"]["rules"];
    REQUIRE(rules.IsArray());
    REQUIRE(rules.Size() == 1);
    CHECK(rules[0].HasMember("action"));
    CHECK(std::string(rules[0]["action"].GetString()) == "route");

    REQUIRE(doc.HasMember("route"));
    CHECK(std::string(doc["route"]["default_domain_resolver"].GetString()) == "dns_resolver");

    REQUIRE(doc.HasMember("ntp"));
    CHECK_FALSE(doc["ntp"].HasMember("detour"));
}

TEST_CASE("subexport sing-box provider fallback via data URL is deterministic and isolated from GLOBAL")
{
    SingBoxModesGuard guard(true);

    std::vector<Proxy> nodes = {makeShadowsocksProxy("LocalNode", "LocalGroup")};
    std::vector<RulesetContent> rulesets;

    const std::string provider_yaml =
        "proxies:\n"
        "  - name: ProviderNode\n"
        "    type: ss\n"
        "    server: 2.2.2.2\n"
        "    port: 443\n"
        "    cipher: aes-128-gcm\n"
        "    password: provider-pass\n";

    ProxyGroupConfig group;
    group.Name = "AUTO";
    group.Type = ProxyGroupType::Select;
    group.UsingProvider = {"pp"};

    extra_settings ext;
    ext.enable_rule_generator = false;
    ext.singbox_version = "1.12.0";
    ext.clash_proxy_providers = {
        ClashProxyProviderConfig{"pp", "http", makeDataUrl(provider_yaml), "providers/pp.yaml", 300},
    };

    const std::string output = proxyToSingBox(nodes, "{}", rulesets, ProxyGroupConfigs{group}, ext);

    rapidjson::Document doc;
    doc.Parse(output.c_str());
    REQUIRE(!doc.HasParseError());

    const rapidjson::Value *provider_proxy = findOutboundByTag(doc, "ProviderNode");
    REQUIRE(provider_proxy != nullptr);

    const rapidjson::Value *auto_group = findOutboundByTag(doc, "AUTO");
    REQUIRE(auto_group != nullptr);
    REQUIRE(auto_group->HasMember("outbounds"));
    CHECK(jsonArrayContainsString((*auto_group)["outbounds"], "ProviderNode"));

    const rapidjson::Value *global_group = findOutboundByTag(doc, "GLOBAL");
    REQUIRE(global_group != nullptr);
    REQUIRE(global_group->HasMember("outbounds"));
    CHECK(jsonArrayContainsString((*global_group)["outbounds"], "DIRECT"));
    CHECK(jsonArrayContainsString((*global_group)["outbounds"], "LocalNode"));
    CHECK_FALSE(jsonArrayContainsString((*global_group)["outbounds"], "ProviderNode"));
}

TEST_CASE("subexport sing-box wireguard outbound obeys 1.13.0 removal gate")
{
    std::vector<RulesetContent> rulesets;
    const ProxyGroupConfigs groups;

    SUBCASE(">=1.13.0 skips wireguard outbounds")
    {
        std::vector<Proxy> nodes = {makeWireGuardProxy("WG-Node")};
        extra_settings ext;
        ext.enable_rule_generator = false;
        ext.nodelist = true;
        ext.singbox_version = "1.13.0";

        const std::string output = proxyToSingBox(nodes, "{}", rulesets, groups, ext);
        rapidjson::Document doc;
        doc.Parse(output.c_str());
        REQUIRE(!doc.HasParseError());

        CHECK(findOutboundByTag(doc, "WG-Node") == nullptr);
    }

    SUBCASE("<1.13.0 keeps wireguard outbounds")
    {
        std::vector<Proxy> nodes = {makeWireGuardProxy("WG-Node")};
        extra_settings ext;
        ext.enable_rule_generator = false;
        ext.nodelist = true;
        ext.singbox_version = "1.12.0";

        const std::string output = proxyToSingBox(nodes, "{}", rulesets, groups, ext);
        rapidjson::Document doc;
        doc.Parse(output.c_str());
        REQUIRE(!doc.HasParseError());

        const rapidjson::Value *wg = findOutboundByTag(doc, "WG-Node");
        REQUIRE(wg != nullptr);
        CHECK(std::string((*wg)["type"].GetString()) == "wireguard");
    }
}
