#pragma once

#include "url.h"
#include "options/options.h"

inline bool matchesDomain(const std::string& host, const std::string& domain)
{
    return host == domain ||
        (host.size() > domain.size() &&
         host.compare(host.size() - domain.size(), domain.size(), domain) == 0 &&
         host[host.size() - domain.size() - 1] == '.');
}

inline bool shouldRedirect(const std::string& host)
{
    for (const char* domain : RedirectHosts)
        if (matchesDomain(host, domain))
            return true;
    return false;
}

inline std::string backendUrl()
{
    std::string result = enablehttps ? "https://" : "http://";
    result += Backend;
    if constexpr ((enablehttps && port != 443) || (!enablehttps && port != 80))
        result += ":" + std::to_string(port);
    return result;
}

inline std::string redirectUrl(const std::string& url)
{
    if (!shouldRedirect(getHost(url)))
        return url;

    const auto start = url.find("://") + 3;
    const auto end = url.find_first_of("/?#", start);
    return backendUrl() + (end == std::string::npos ? "" : url.substr(end));
}
