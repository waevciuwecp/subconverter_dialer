#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "generator/template/templates.h"
#include "handler/settings.h"
#include "helpers/test_helpers.h"
#include "utils/file.h"
#include "utils/yamlcpp_extra.h"

namespace
{

struct ScopedCurrentPath
{
    explicit ScopedCurrentPath(const std::filesystem::path &target) : old(std::filesystem::current_path())
    {
        std::filesystem::current_path(target);
    }

    ~ScopedCurrentPath()
    {
        std::filesystem::current_path(old);
    }

    std::filesystem::path old;
};

std::filesystem::path makeTempDir(const std::string &name)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / ("subconverter-template-vectors-" + name);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

RulesetContent makeRulesetContent(const std::string &group, int type, std::string path, std::string typed_path, std::string content, int interval = 0)
{
    RulesetContent rc;
    rc.rule_group = group;
    rc.rule_type = type;
    rc.rule_path = std::move(path);
    rc.rule_path_typed = std::move(typed_path);
    rc.rule_content = makeReadyFuture(std::move(content));
    rc.update_interval = interval;
    return rc;
}

bool hasRule(const YAML::Node &rules, const std::string &needle)
{
    if(!rules || !rules.IsSequence())
        return false;
    for(const auto &rule : rules)
    {
        if(rule.IsScalar() && rule.as<std::string>() == needle)
            return true;
    }
    return false;
}

template_args makeMinimalTemplateArgs()
{
    template_args vars;
    vars.request_params["seed"] = "1";
    return vars;
}

} // namespace

TEST_CASE("templates render_template covers callback-heavy deterministic vector")
{
    const std::string old_prefix = global.managedConfigPrefix;
    global.managedConfigPrefix = "https://managed.test";

    template_args vars;
    vars.global_vars["service.name"] = "svc";
    vars.request_params["a"] = "1";
    vars.request_params["empty"] = "";
    vars.request_params["z"] = "9";
    vars.local_vars["t"] = "  abc  ";

    const std::string input = R"({{ UrlEncode("a b") }}|{{ UrlDecode("a%2Bb") }}|{{ trim_of("..abc..","") }}|{{ trim_of("..abc..",".") }}|{{ trim(local.t) }}|{{ replace("abc","","x") }}|{% if find("abc123","[0-9]+") %}Y{% else %}N{% endif %}|{{ set("local.mut","A") }}{{ append("local.mut","B") }}{{ append("local.new","C") }}{{ split("x|y||z","|","local.parts") }}{{ local.mut }}|{{ local.new }}|{{ local.parts.0 }}|{{ local.parts.1 }}|{{ local.parts.2 }}|{{ local.parts.3 }}|{% if startsWith("foobar","foo") %}1{% else %}0{% endif %}{% if endsWith("foobar","bar") %}1{% else %}0{% endif %}|{{ string(bool("TrUe")) }}{{ string(bool("0")) }}|{{ getLink("/api") }}|{{ request._args }}|{% if exists("request.empty") %}E{% else %}M{% endif %}|{{ request.a }}|{{ request.z }})";

    std::string output;
    REQUIRE(render_template(input, vars, output, "") == 0);
    CHECK(output == "a%20b|a+b|..abc..|abc|abc|abc|Y|AB|C|x|y||z|11|10|https://managed.test/api|a=1&empty&z=9|M|1|9");

    global.managedConfigPrefix = old_prefix;
}

TEST_CASE("templates render_template include scope enforcement vectors")
{
    const std::filesystem::path root = makeTempDir("include");
    const std::filesystem::path scope_dir = root / "scope";
    std::filesystem::create_directories(scope_dir);

    REQUIRE(fileWrite((scope_dir / "inside.tpl").string(), "INSIDE", true) == 0);
    REQUIRE(fileWrite((root / "outside.tpl").string(), "OUTSIDE", true) == 0);

    template_args vars = makeMinimalTemplateArgs();
    ScopedCurrentPath cwd(root);
    std::string output;

    SUBCASE("allowed include within scope")
    {
        REQUIRE(render_template(R"({% include "scope/inside.tpl" %})", vars, output, "scope") == 0);
        CHECK(output == "INSIDE");
    }

    SUBCASE("denied include outside scope")
    {
        REQUIRE(render_template(R"({% include "outside.tpl" %})", vars, output, "scope") == -1);
        CHECK(containsText(output, "out of scope"));
    }

    SUBCASE("invalid include scope falls back to no scope restriction")
    {
        REQUIRE(render_template(R"({% include "outside.tpl" %})", vars, output, "missing-scope") == 0);
        CHECK(output == "OUTSIDE");
    }

    std::filesystem::remove_all(root);
}

TEST_CASE("templates render_template returns failure message for invalid template")
{
    template_args vars = makeMinimalTemplateArgs();
    std::string output;
    REQUIRE(render_template("{% if request.seed %}", vars, output, "") == -1);
    CHECK(containsText(output, "Template render failed! Reason:"));
}

TEST_CASE("templates renderClashScript decodes provider names and resolves collisions")
{
    YAML::Node base_rule;
    std::vector<RulesetContent> rulesets = {
        makeRulesetContent("ProxyA", RULESET_CLASH_CLASSICAL, "https://example.test/encoded%20name.list", "https://example.test/encoded%20name.list", ""),
        makeRulesetContent("ProxyB", RULESET_CLASH_CLASSICAL, "https://example.test/encoded%20name.yaml", "https://example.test/encoded%20name.yaml", "")
    };

    REQUIRE(renderClashScript(base_rule, rulesets, "https://managed.test", false, true, true) == 0);
    REQUIRE(base_rule["rules"].IsSequence());
    CHECK(hasRule(base_rule["rules"], "RULE-SET,encoded name,ProxyA"));
    CHECK(hasRule(base_rule["rules"], "RULE-SET,encoded name 2,ProxyB"));
    CHECK(base_rule["rule-providers"]["encoded name"]["behavior"].as<std::string>() == "classical");
    CHECK(base_rule["rule-providers"]["encoded name 2"]["behavior"].as<std::string>() == "classical");
}

TEST_CASE("templates renderClashScript keeps no-resolve in generated IP-CIDR rule-set")
{
    YAML::Node base_rule;
    std::vector<RulesetContent> rulesets = {
        makeRulesetContent("Proxy", RULESET_SURGE, "https://source.test/nres.list", "https://source.test/nres.list",
                           "IP-CIDR,1.2.3.0/24,no-resolve\nDOMAIN,example.com\n")
    };

    REQUIRE(renderClashScript(base_rule, rulesets, "https://managed.test", false, true, false) == 0);
    REQUIRE(base_rule["rules"].IsSequence());
    CHECK(hasRule(base_rule["rules"], "RULE-SET,nres (Domain),Proxy"));
    CHECK(hasRule(base_rule["rules"], "RULE-SET,nres (IP-CIDR),Proxy,no-resolve"));
}

TEST_CASE("templates renderClashScript script mode handles FINAL and GEOIP inline vectors")
{
    YAML::Node base_rule;
    std::vector<RulesetContent> rulesets = {
        makeRulesetContent("Proxy", RULESET_CLASH_DOMAIN, "https://example.test/domain.list", "https://example.test/domain.list", ""),
        makeRulesetContent("DIRECT", RULESET_SURGE, "", "", "[]FINAL"),
        makeRulesetContent("CN", RULESET_SURGE, "", "", "[]GEOIP,CN")
    };

    REQUIRE(renderClashScript(base_rule, rulesets, "", true, true, false) == 0);
    REQUIRE(base_rule["script"]["code"].IsScalar());
    const std::string script = base_rule["script"]["code"].as<std::string>();
    CHECK(containsText(script, "geoips = { \"CN\": \"CN\" }"));
    CHECK(containsText(script, "return \"DIRECT\""));
}
