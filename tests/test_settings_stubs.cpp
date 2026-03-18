#include <future>
#include <string>
#include <utility>

#include "generator/template/templates.h"
#include "handler/interfaces.h"
#include "handler/multithread.h"
#include "handler/settings.h"
#include "script/cron.h"
#include "server/webserver.h"
#include "utils/file.h"

WebServer webServer;

std::string parseProxy(const std::string &source)
{
    return source;
}

void refresh_schedule()
{
}

RegexMatchConfigs safe_get_emojis()
{
    return global.emojis;
}

RegexMatchConfigs safe_get_renames()
{
    return global.renames;
}

RegexMatchConfigs safe_get_streams()
{
    return global.streamNodeRules;
}

RegexMatchConfigs safe_get_times()
{
    return global.timeNodeRules;
}

YAML::Node safe_get_clash_base()
{
    return YAML::Node(YAML::NodeType::Map);
}

INIReader safe_get_mellow_base()
{
    return INIReader();
}

void safe_set_emojis(RegexMatchConfigs data)
{
    global.emojis = std::move(data);
}

void safe_set_renames(RegexMatchConfigs data)
{
    global.renames = std::move(data);
}

void safe_set_streams(RegexMatchConfigs data)
{
    global.streamNodeRules = std::move(data);
}

void safe_set_times(RegexMatchConfigs data)
{
    global.timeNodeRules = std::move(data);
}

std::shared_future<std::string> fetchFileAsync(const std::string &path, const std::string &, int, bool find_local, bool)
{
    std::promise<std::string> promise;
    if(find_local && fileExist(path, true))
        promise.set_value(fileGet(path, true));
    else
        promise.set_value("");
    return promise.get_future().share();
}

std::string fetchFile(const std::string &path, const std::string &proxy, int cache_ttl, bool find_local)
{
    (void)proxy;
    (void)cache_ttl;
    if(find_local && fileExist(path, true))
        return fileGet(path, true);
    return "";
}
