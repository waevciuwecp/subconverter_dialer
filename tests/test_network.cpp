#include <doctest/doctest.h>

#include <string>

#include "utils/network.h"

TEST_CASE("network IP validators")
{
    CHECK(isIPv4("192.168.1.1"));
    CHECK_FALSE(isIPv4("256.1.1.1"));
    CHECK(isIPv6("2001:db8::1"));
    CHECK_FALSE(isIPv6("2001:::1"));
}

TEST_CASE("network urlParse handles scheme host port and path")
{
    std::string url = "https://example.com/api?q=1";
    std::string host;
    std::string path;
    int port = 0;
    bool is_tls = false;
    urlParse(url, host, path, port, is_tls);
    CHECK(host == "example.com");
    CHECK(path == "/api?q=1");
    CHECK(port == 443);
    CHECK(is_tls);

    std::string url_v6 = "http://[2001:db8::1]:8443/a";
    is_tls = false;
    urlParse(url_v6, host, path, port, is_tls);
    CHECK(host == "2001:db8::1");
    CHECK(path == "/a");
    CHECK(port == 8443);
    CHECK_FALSE(is_tls);
}

TEST_CASE("network isLink covers accepted schemes")
{
    CHECK(isLink("http://example.com"));
    CHECK(isLink("https://example.com"));
    CHECK(isLink("data:text/plain,abc"));
    CHECK_FALSE(isLink("ftp://example.com"));
}
