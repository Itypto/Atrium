#pragma once

#include <cstdlib>
#include <string>

namespace Atrium::Certificates
{
    inline std::string caBundlePath()
    {
        const char* path = std::getenv("ATRIUM_CA_BUNDLE");
        return path ? path : "";
    }

    inline bool useCurlTlsContext()
    {
        const char* value = std::getenv("ATRIUM_USE_CURL_TLS_CONTEXT");
        return value && std::string(value) == "1";
    }
}
