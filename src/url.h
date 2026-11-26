#pragma once

#include <string>

inline std::string getHost(const std::string& url)
{
    const auto scheme = url.find("://");
    if (scheme == std::string::npos)
        return {};

    std::string protocol = url.substr(0, scheme);
    for (char& c : protocol)
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (protocol != "http" && protocol != "https")
        return {};

    const auto start = scheme + 3;
    const auto end = url.find_first_of("/?#", start);
    std::string authority = url.substr(start, end - start);
    if (authority.empty() || authority.find_first_of("@[] \t\r\n") != std::string::npos)
        return {};

    const auto colon = authority.find(':');
    if (colon != std::string::npos)
    {
        if (colon == 0 || colon + 1 == authority.size())
            return {};
        unsigned int portValue = 0;
        for (auto i = colon + 1; i < authority.size(); ++i)
        {
            if (authority[i] < '0' || authority[i] > '9') return {};
            portValue = portValue * 10 + static_cast<unsigned int>(authority[i] - '0');
            if (portValue > 65535) return {};
        }
        authority.resize(colon);
    }

    for (char& c : authority)
    {
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '-' || c == '.'))
            return {};
    }
    return authority;
}
