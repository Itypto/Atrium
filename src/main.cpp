#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>

#include <string>

#include "hook.h"
#include "hud/hud.h"
#include "options/options.h"
#include "url.h"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Atrium", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Atrium", __VA_ARGS__)

static void* initialize(void*)
{
    void* library = nullptr;
    unsigned int attempts = 0;
    while (!library)
    {
        library = dlopen("libUE4.so", RTLD_NOW | RTLD_NOLOAD);
        if (!library)
        {
            if (attempts++ % 100 == 0)
                LOGI("Waiting for libUE4.so before installing hooks");
            usleep(100000);
        }
    }
    if (!installHooks(library))
        LOGE("Initialization failed; redirects are unavailable");
    else
        LOGI("Curl hooks installed");
    Atrium::Hud::initialize();
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*)
{
    const std::string backend = Backend;
    if (backend.empty() || getHost("https://" + backend) != backend)
    {
        LOGE("Backend must be a lowercase hostname without a scheme or port");
        return JNI_ERR;
    }

    void* library = dlopen("libUE4.so", RTLD_NOW | RTLD_NOLOAD);
    if (library)
    {
        if (!installHooks(library))
            LOGE("Initialization failed; redirects are unavailable");
        else
            LOGI("Curl hooks installed");
        Atrium::Hud::initialize();
        return JNI_VERSION_1_6;
    }

    pthread_t thread;
    const int error = pthread_create(&thread, nullptr, initialize, nullptr);
    if (error != 0)
    {
        LOGE("Cannot start initialization thread: %d", error);
        return JNI_ERR;
    }
    pthread_detach(thread);
    return JNI_VERSION_1_6;
}
