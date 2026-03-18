#ifndef SUBCONVERTER_TEST_HELPERS_H_INCLUDED
#define SUBCONVERTER_TEST_HELPERS_H_INCLUDED

#include <future>
#include <string>

#include "parser/config/proxy.h"

inline Proxy makeProxyWithRemark(const std::string &remark)
{
    Proxy node;
    node.Remark = remark;
    node.Group = "UnitTestGroup";
    node.GroupId = 1;
    return node;
}

inline std::shared_future<std::string> makeReadyFuture(std::string value)
{
    std::promise<std::string> promise;
    promise.set_value(std::move(value));
    return promise.get_future().share();
}

inline bool containsText(const std::string &haystack, const std::string &needle)
{
    return haystack.find(needle) != std::string::npos;
}

#endif // SUBCONVERTER_TEST_HELPERS_H_INCLUDED
