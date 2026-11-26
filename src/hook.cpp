#include <android/log.h>
#include <curl/curl.h>
#include <dobby/dobby.h>
#include <dlfcn.h>

#include <cstdint>

#include "curl_redirect.h"
#include "hook.h"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Atrium", __VA_ARGS__)

using Init = CURL* (*)();
using Cleanup = void (*)(CURL*);
using Reset = void (*)(CURL*);
using Duplicate = CURL* (*)(CURL*);

extern "C" {
__attribute__((visibility("hidden"))) void* atrium_original_setopt = nullptr;
__attribute__((visibility("hidden"))) CURLcode atrium_setopt_hook(CURL*, CURLoption, ...);
}

static Init originalInit = nullptr;
static Cleanup originalCleanup = nullptr;
static Reset originalReset = nullptr;
static Duplicate originalDuplicate = nullptr;

static CurlRedirect<verifytls> redirector;

static Setopt originalSetopt()
{
    return reinterpret_cast<Setopt>(atrium_original_setopt);
}

static CURL* hookedInit()
{
    CURL* handle = originalInit();
    if (handle)
        redirector.created(handle);
    return handle;
}

static void hookedCleanup(CURL* handle)
{
    redirector.cleaned(handle);
    originalCleanup(handle);
}

static void hookedReset(CURL* handle)
{
    originalReset(handle);
    redirector.reset(handle);
}

static CURL* hookedDuplicate(CURL* handle)
{
    CURL* duplicate = originalDuplicate(handle);
    if (duplicate)
        redirector.duplicated(handle, duplicate);
    return duplicate;
}

extern "C" __attribute__((visibility("hidden")))
CURLcode atrium_handle_setopt(CURL* handle, CURLoption option, std::uintptr_t argument)
{
    bool unknownHandle = false;
    const CURLcode result = redirector.setopt(originalSetopt(), handle, option,
                                              argument, unknownHandle);
    if (unknownHandle && option == CURLOPT_URL)
        LOGE("Skipping URL on a curl handle created before hook installation");
    if (result != CURLE_OK)
        LOGE("curl option %d failed: %d", static_cast<int>(option), static_cast<int>(result));
    return result;
}

template <typename T>
static bool install(void* library, const char* name, T replacement, T& original)
{
    dlerror();
    void* target = dlsym(library, name);
    const char* error = dlerror();
    if (error || !target)
    {
        LOGE("Cannot find %s: %s", name, error ? error : "symbol is null");
        return false;
    }
    if (DobbyHook(target, reinterpret_cast<void*>(replacement),
                  reinterpret_cast<void**>(&original)) != 0 || !original)
    {
        LOGE("Failed to hook %s", name);
        return false;
    }
    return true;
}

bool installHooks(void* library)
{
    if (!install(library, "curl_easy_init", hookedInit, originalInit) ||
        !install(library, "curl_easy_cleanup", hookedCleanup, originalCleanup) ||
        !install(library, "curl_easy_reset", hookedReset, originalReset) ||
        !install(library, "curl_easy_duphandle", hookedDuplicate, originalDuplicate))
        return false;

    dlerror();
    void* target = dlsym(library, "curl_easy_setopt");
    const char* error = dlerror();
    if (error || !target)
    {
        LOGE("Cannot find curl_easy_setopt: %s", error ? error : "symbol is null");
        return false;
    }
    if (DobbyHook(target, reinterpret_cast<void*>(atrium_setopt_hook),
                  &atrium_original_setopt) != 0 || !atrium_original_setopt)
    {
        LOGE("Failed to hook curl_easy_setopt");
        return false;
    }
    return true;
}
