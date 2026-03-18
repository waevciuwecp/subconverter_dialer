#include <doctest/doctest.h>

#include <sstream>
#include <string>
#include <vector>

#include "handler/settings.h"

int loadExternalYAML(YAML::Node &node, ExternalConfig &ext);
int loadExternalTOML(toml::value &root, ExternalConfig &ext);

namespace
{
toml::value parseTomlText(const std::string &content, const std::string &filename)
{
    std::istringstream stream(content);
    return toml::parse(stream, filename);
}

const ClashProxyProviderConfig *findProviderByName(const std::vector<ClashProxyProviderConfig> &providers,
                                                   const std::string &name)
{
    for(const auto &provider : providers)
    {
        if(provider.Name == name)
            return &provider;
    }
    return nullptr;
}

} // namespace

TEST_CASE("settings parseGroupTimes handles complete, partial, and sparse vectors")
{
    int interval = 0;
    int timeout = 0;
    int tolerance = 0;

    parseGroupTimes("15,30,45", &interval, &timeout, &tolerance);
    CHECK(interval == 15);
    CHECK(timeout == 30);
    CHECK(tolerance == 45);

    interval = 1;
    timeout = 2;
    tolerance = 3;
    parseGroupTimes("9", &interval, &timeout, &tolerance);
    CHECK(interval == 9);
    CHECK(timeout == 2);
    CHECK(tolerance == 3);

    interval = 7;
    timeout = 8;
    tolerance = 9;
    parseGroupTimes("100,200,300", nullptr, &timeout, nullptr);
    CHECK(interval == 7);
    CHECK(timeout == 200);
    CHECK(tolerance == 9);
}

TEST_CASE("settings loadExternalTOML parses provider table defaults and sanitization")
{
    const std::string content = R"(version = 1
[custom]
use_dialer = true
dialer_group_name = "dialer-main"
apply_dialer_to = "all"
singbox_version = "1.14.0"

[custom.proxy_providers."team/a"]
url = "https://example.com/a.yaml"
interval = 0
type = ""
path = ""

[custom.proxy_providers.named]
name = "Named Provider"
url = "https://example.com/b.yaml"
interval = 120
type = ""
path = ""
)";

    toml::value root = parseTomlText(content, "ext.toml");
    ExternalConfig ext;

    REQUIRE(loadExternalTOML(root, ext) == 0);
    CHECK(ext.use_dialer.get(false));
    CHECK(ext.dialer_group_name == "dialer-main");
    CHECK(ext.apply_dialer_to == "all");
    CHECK(ext.singbox_version == "1.14.0");

    REQUIRE(ext.clash_proxy_providers.size() == 2);

    const auto *team = findProviderByName(ext.clash_proxy_providers, "team/a");
    REQUIRE(team != nullptr);
    CHECK(team->Type == "http");
    CHECK(team->Interval == 3600);
    CHECK(team->Path == "./proxy_provider/team_a.yaml");

    const auto *named = findProviderByName(ext.clash_proxy_providers, "Named Provider");
    REQUIRE(named != nullptr);
    CHECK(named->Type == "http");
    CHECK(named->Interval == 120);
    CHECK(named->Path == "./proxy_provider/Named_Provider.yaml");
}

TEST_CASE("settings loadExternalTOML accepts root-level proxy-providers and skips incomplete entries")
{
    const std::string content = R"(version = 1
[custom]

[[proxy-providers]]
name = "Root Provider"
url = "https://example.com/root.yaml"
interval = -9
type = ""
path = ""

[[proxy-providers]]
url = "https://example.com/skip.yaml"
)";

    toml::value root = parseTomlText(content, "root-providers.toml");
    ExternalConfig ext;

    REQUIRE(loadExternalTOML(root, ext) == 0);
    REQUIRE(ext.clash_proxy_providers.size() == 1);
    CHECK(ext.clash_proxy_providers[0].Name == "Root Provider");
    CHECK(ext.clash_proxy_providers[0].Type == "http");
    CHECK(ext.clash_proxy_providers[0].Interval == 3600);
    CHECK(ext.clash_proxy_providers[0].Path == "./proxy_provider/Root_Provider.yaml");
}

TEST_CASE("settings loadExternalTOML rejects invalid proxy provider payload shapes")
{
    const std::string content = R"(version = 1
[custom]
proxy_providers = [1]
)";

    toml::value root = parseTomlText(content, "invalid-providers.toml");
    ExternalConfig ext;

    CHECK(loadExternalTOML(root, ext) == -1);
    CHECK(ext.clash_proxy_providers.empty());
}

TEST_CASE("settings loadExternalYAML parses provider variants and validates invalid payloads")
{
    SUBCASE("map style supports defaults and sanitized fallback path")
    {
        const std::string content = R"(custom:
  use_dialer: true
  dialer_group_name: dialer-yaml
  apply_dialer_to: all
  singbox_version: 1.13.1
  proxy_providers:
    "team/a":
      url: https://example.com/a.yaml
      interval: 0
      type: ""
      path: ""
    alias:
      name: Alias Name
      url: https://example.com/b.yaml
      interval: 45
      type: ""
      path: ""
)";

        YAML::Node root = YAML::Load(content);
        ExternalConfig ext;

        REQUIRE(loadExternalYAML(root, ext) == 0);
        CHECK(ext.use_dialer.get(false));
        CHECK(ext.dialer_group_name == "dialer-yaml");
        CHECK(ext.apply_dialer_to == "all");
        CHECK(ext.singbox_version == "1.13.1");

        REQUIRE(ext.clash_proxy_providers.size() == 2);
        const auto *team = findProviderByName(ext.clash_proxy_providers, "team/a");
        REQUIRE(team != nullptr);
        CHECK(team->Type == "http");
        CHECK(team->Interval == 3600);
        CHECK(team->Path == "./proxy_provider/team_a.yaml");

        const auto *alias = findProviderByName(ext.clash_proxy_providers, "Alias Name");
        REQUIRE(alias != nullptr);
        CHECK(alias->Type == "http");
        CHECK(alias->Interval == 45);
        CHECK(alias->Path == "./proxy_provider/Alias_Name.yaml");
    }

    SUBCASE("proxy-providers sequence key is accepted")
    {
        const std::string content = R"(custom:
  proxy-providers:
    - name: Sequence Provider
      url: https://example.com/seq.yaml
)";

        YAML::Node root = YAML::Load(content);
        ExternalConfig ext;

        REQUIRE(loadExternalYAML(root, ext) == 0);
        REQUIRE(ext.clash_proxy_providers.size() == 1);
        CHECK(ext.clash_proxy_providers[0].Name == "Sequence Provider");
        CHECK(ext.clash_proxy_providers[0].Path == "./proxy_provider/Sequence_Provider.yaml");
    }

    SUBCASE("scalar proxy_providers payload is rejected")
    {
        const std::string content = R"(custom:
  proxy_providers: invalid
)";

        YAML::Node root = YAML::Load(content);
        ExternalConfig ext;

        CHECK(loadExternalYAML(root, ext) == -1);
        CHECK(ext.clash_proxy_providers.empty());
    }
}
