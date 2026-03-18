#include <doctest/doctest.h>

#include <string>

#include "generator/template/templates.h"

#if __has_include(<jinja2cpp/template.h>) && __has_include(<jinja2cpp/user_callable.h>) && __has_include(<jinja2cpp/binding/nlohmann_json.h>)

std::string jinja2_webGet(const std::string &url)
{
    return "FETCH:" + url;
}

#define render_template render_template_jinja2_backend
#include "generator/template/template_jinja2.cpp"
#undef render_template

TEST_CASE("template_jinja2 render_template deterministic callback vector")
{
    template_args vars;
    vars.global_vars["service.name"] = "alpha";
    vars.request_params["q"] = "42";
    vars.local_vars["path"] = "/v1";

    std::string output;
    const int rc = render_template_jinja2_backend(
        "{{ global.service.name }}|{{ request.q }}|{{ local.path }}|{{ replace('abc123', '[0-9]+', '') }}|{{ fetch('https://u.test') }}",
        vars, output, "ignored-scope");
    REQUIRE(rc == 0);
    CHECK(output == "alpha|42|/v1|abc|FETCH:https://u.test");
}

TEST_CASE("template_jinja2 include_scope argument does not affect rendering")
{
    template_args vars;
    vars.request_params["k"] = "v";

    const std::string input = "{{ request.k }}";
    std::string out_a, out_b;
    REQUIRE(render_template_jinja2_backend(input, vars, out_a, "scope-a") == 0);
    REQUIRE(render_template_jinja2_backend(input, vars, out_b, "scope-b") == 0);
    CHECK(out_a == "v");
    CHECK(out_b == out_a);
}

#else

TEST_CASE("template_jinja2 vectors are skipped when jinja2cpp is unavailable")
{
    CHECK(true);
}

#endif
