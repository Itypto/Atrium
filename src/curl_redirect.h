#pragma once

#include <curl/curl.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "redirect.h"
#include "certificates.h"

using Setopt = CURLcode (*)(CURL*, CURLoption, ...);

template <bool VerifyTls>
class CurlRedirect
{
    struct HandleOptions
    {
        long port = 0;
        long verifyPeer = 1;
        long verifyHost = 2;
        bool redirected = false;
        std::optional<std::string> caInfo;
        bool caOverridden = false;
        curl_ssl_ctx_callback sslContext = nullptr;
        bool sslContextOverridden = false;
    };

    std::mutex mutex_;
    std::unordered_map<CURL*, HandleOptions> handles_;

public:
    void created(CURL* handle)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handles_[handle] = {};
    }

    void cleaned(CURL* handle)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handles_.erase(handle);
    }

    void reset(CURL* handle)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handles_.find(handle);
        if (it != handles_.end())
            it->second = {};
    }

    void duplicated(CURL* source, CURL* duplicate)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handles_.find(source);
        if (it != handles_.end())
            handles_[duplicate] = it->second;
        else
            handles_.erase(duplicate);
    }

    CURLcode setopt(Setopt original, CURL* handle, CURLoption option,
                    std::uintptr_t argument, bool& unknownHandle)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handles_.find(handle);
        if (it == handles_.end())
        {
            unknownHandle = true;
            if (option == CURLOPT_URL)
                return original(handle, option, reinterpret_cast<const char*>(argument));
            if (option == CURLOPT_CAINFO)
                return original(handle, option, reinterpret_cast<const char*>(argument));
            if (option == CURLOPT_SSL_CTX_FUNCTION)
                return original(handle, option, reinterpret_cast<curl_ssl_ctx_callback>(argument));
            return original(handle, option, static_cast<long>(argument));
        }

        auto& state = it->second;
        if (option != CURLOPT_URL)
        {
            if (option == CURLOPT_SSL_CTX_FUNCTION)
            {
                const auto requested = reinterpret_cast<curl_ssl_ctx_callback>(argument);
                const bool overrideContext = state.redirected && VerifyTls && enablehttps &&
                    Atrium::Certificates::useCurlTlsContext();
                const CURLcode result = original(handle, option, overrideContext ? nullptr : requested);
                if (result == CURLE_OK)
                {
                    state.sslContext = requested;
                    state.sslContextOverridden = overrideContext;
                }
                return result;
            }
            if (option == CURLOPT_CAINFO)
            {
                const char* requested = reinterpret_cast<const char*>(argument);
                const auto ca = state.redirected && VerifyTls && enablehttps
                    ? Atrium::Certificates::caBundlePath() : std::string{};
                const auto result = original(handle, option, ca.empty() ? requested : ca.c_str());
                if (result == CURLE_OK)
                {
                    state.caInfo = requested ? std::optional<std::string>(requested) : std::nullopt;
                    state.caOverridden = !ca.empty();
                }
                return result;
            }
            const long requested = static_cast<long>(argument);
            long effective = requested;
            if (state.redirected && option == CURLOPT_PORT)
                effective = 0;
            if (state.redirected && option == CURLOPT_SSL_VERIFYPEER)
                effective = VerifyTls ? 1 : 0;
            if (state.redirected && option == CURLOPT_SSL_VERIFYHOST)
                effective = VerifyTls ? 2 : 0;

            const CURLcode result = original(handle, option, effective);
            if (result == CURLE_OK)
            {
                if (option == CURLOPT_PORT) state.port = requested;
                if (option == CURLOPT_SSL_VERIFYPEER) state.verifyPeer = requested;
                if (option == CURLOPT_SSL_VERIFYHOST) state.verifyHost = requested;
            }
            return result;
        }

        const char* url = reinterpret_cast<const char*>(argument);
        if (!url)
            return original(handle, option, url);

        const std::string input(url);
        const bool redirect = shouldRedirect(getHost(input));
        const std::string rewritten = redirect ? redirectUrl(input) : std::string{};
        CURLcode result = original(handle, option, redirect ? rewritten.c_str() : url);
        if (result != CURLE_OK)
            return result;

        const bool wasRedirected = state.redirected;
        state.redirected = redirect;
        if (redirect || wasRedirected)
        {
            result = original(handle, CURLOPT_PORT, redirect ? 0L : state.port);
            if (result == CURLE_OK)
                result = original(handle, CURLOPT_SSL_VERIFYPEER,
                                  redirect ? (VerifyTls ? 1L : 0L) : state.verifyPeer);
            if (result == CURLE_OK)
                result = original(handle, CURLOPT_SSL_VERIFYHOST,
                                  redirect ? (VerifyTls ? 2L : 0L) : state.verifyHost);
            if (result == CURLE_OK)
            {
                const bool overrideContext = redirect && VerifyTls && enablehttps &&
                    Atrium::Certificates::useCurlTlsContext();
                if (overrideContext || state.sslContextOverridden)
                {
                    result = original(handle, CURLOPT_SSL_CTX_FUNCTION,
                                      overrideContext ? nullptr : state.sslContext);
                    if (result == CURLE_OK) state.sslContextOverridden = overrideContext;
                }
            }
            if (result == CURLE_OK && redirect && VerifyTls && enablehttps)
            {
                const auto ca = Atrium::Certificates::caBundlePath();
                if (!ca.empty())
                {
                    result = original(handle, CURLOPT_CAINFO, ca.c_str());
                    if (result == CURLE_OK) state.caOverridden = true;
                }
            }
            else if (result == CURLE_OK && state.caOverridden)
            {
                result = original(handle, CURLOPT_CAINFO, state.caInfo ? state.caInfo->c_str() : nullptr);
                if (result == CURLE_OK) state.caOverridden = false;
            }
        }
        return result;
    }
};
