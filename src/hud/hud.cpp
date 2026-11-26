#include "hud.h"
#include "../options/options.h"
#include "extension.h"
#include "playlist.h"
#include <android/log.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dobby/dobby.h>
#include <elf.h>
#include <link.h>
#include <mutex>
#include <pthread.h>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
namespace Atrium::Hud
{
namespace
{
uintptr_t base;
bool unsupportedBuild;
using Event = void (*)(void *, void *, void *);
Event original;
std::atomic<int> gameThread{0};
thread_local bool busy;
uint64_t nextScan;
uint32_t playlistEvents[3]{};
constexpr uint8_t buildId[] = {0xbf, 0xf2, 0x88, 0x62, 0xf5, 0xe4, 0x5e, 0x5a, 0xa7, 0xeb,
                               0xae, 0xd3, 0x0b, 0xab, 0x11, 0xcc, 0x96, 0xc8, 0x3b, 0x00};
void log(const char *text) { __android_log_print(ANDROID_LOG_INFO, "AtriumHUD", "%s", text); }
uintptr_t untag(uintptr_t p) { return p & 0x00ffffffffffffffull; }
template <class T> T read(uintptr_t p)
{
    T value{};
    if (p)
        memcpy(&value, reinterpret_cast<void *>(untag(p)), sizeof(value));
    return value;
}
uintptr_t pointer(uintptr_t p) { return untag(read<uintptr_t>(p)); }
uint64_t now()
{
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000ull + t.tv_nsec / 1000000;
}
std::string name(uintptr_t object)
{
    if (!object)
        return {};
    const auto index = read<uint32_t>(object + 0x18);
    const auto block = index >> 16;
    if (block > read<uint32_t>(base + 0xdf85878) || block >= 8192)
        return {};
    const auto memory = pointer(base + 0xdf85880 + block * 8);
    if (!memory)
        return {};
    const auto entry = memory + (index & 65535) * 2;
    const auto header = read<uint16_t>(entry);
    const auto length = header >> 6;
    if (!length || length > 255)
        return {};
    std::string out;
    out.reserve(length);
    for (int i = 0; i < length; ++i)
    {
        unsigned c = header & 1 ? read<uint16_t>(entry + 2 + i * 2) : read<uint8_t>(entry + 2 + i);
        out.push_back(c < 128 ? char(c) : '?');
    }
    return out;
}
uintptr_t objectAt(uint32_t index)
{
    auto chunks = pointer(base + 0xdf7ec80);
    if (!chunks)
        return 0;
    auto chunk = pointer(chunks + (index / 65536) * 8);
    return chunk ? pointer(chunk + (index % 65536) * 24) : 0;
}
int property(uintptr_t object, const char *wanted, const char *kind, int elementSize)
{
    auto cls = pointer(object + 0x10);
    for (int depth = 0; cls && depth < 40; ++depth, cls = pointer(cls + 0x40))
    {
        auto field = pointer(cls + 0x48);
        for (int i = 0; field && i < 4096; ++i, field = pointer(field + 0x28))
            if (name(field) == wanted)
            {
                const auto offset = read<int>(field + 0x44);
                if (name(pointer(field + 0x10)) != kind || read<int>(field + 0x34) != elementSize)
                    return 0;
                const auto size = read<int>(pointer(object + 0x10) + 0x50);
                return offset >= 0x28 && offset + elementSize <= size ? offset : 0;
            }
    }
    return 0;
}
bool registerWidget(uintptr_t playlist, Kind kind)
{
    const auto path = widgetPath(kind);
    if (!path)
        return false;
    const auto offset = property(playlist, "UIExtensions", "ArrayProperty", 16);
    if (!offset)
    {
        log("UIExtensions reflection validation failed; playlist left unchanged.");
        return false;
    }
    if (kind == Kind::Arena)
    {
        const auto aspect = property(playlist, "MaximumAspectRatio", "FloatProperty", 4);
        if (aspect && read<float>(playlist + aspect) > 0)
            *reinterpret_cast<float *>(playlist + aspect) = 0;
    }
    static Name resolvedPaths[3]{};
    auto &resolved = resolvedPaths[static_cast<unsigned>(kind)];
    if (!resolved.Index)
    {
        const auto length = static_cast<int>(std::char_traits<char16_t>::length(path)) + 1;
        struct EngineString
        {
            const char16_t *data;
            int num;
            int max;
        } text{path, length, length};
        static_assert(sizeof(EngineString) == 16);
        const auto key = reinterpret_cast<uint64_t (*)(const EngineString *)>(base + 0x62c0ec4)(&text);
        resolved = {uint32_t(key), uint32_t(key >> 32)};
    }
    if (!resolved.Index)
    {
        log("Engine returned no scoring widget path name.");
        return false;
    }
    auto &array = *reinterpret_cast<Array *>(playlist + offset);
    const auto result = Append(array, resolved,
                               [](void *memory, size_t bytes)
                               {
                                   return reinterpret_cast<void *(*)(void *, size_t, uint32_t)>(
                                       base + 0x6264f58)(memory, bytes, 8);
                               });
    if (result == Result::Added)
        __android_log_print(ANDROID_LOG_INFO, "AtriumHUD",
                            "Registered %s HUD on %s; existing HUD and input retained.",
                            kind == Kind::Arena ? "Arena" : "Competitive", name(playlist).c_str());
    else if (result != Result::Present)
        __android_log_print(ANDROID_LOG_ERROR, "AtriumHUD",
                            "Widget registration rejected (result=%d, num=%d, max=%d).",
                            static_cast<int>(result), array.Num, array.Max);
    return result == Result::Added || result == Result::Present;
}
void scan(bool force)
{
    struct CachedPlaylist
    {
        uintptr_t object;
        uint32_t index;
        Kind kind;
    };
    static uintptr_t playlistClass = 0;
    static std::vector<CachedPlaylist> cached;
    static uint64_t nextFullScan = 0;
    if (!force && !cached.empty() && now() < nextFullScan)
    {
        bool valid = true;
        for (const auto &entry : cached)
        {
            if (objectAt(entry.index) != entry.object || (read<uint32_t>(entry.object + 8) & 0x18000))
            {
                valid = false;
                continue;
            }
            if (!registerWidget(entry.object, entry.kind))
                valid = false;
        }
        if (valid)
            return;
    }
    const auto count = read<int>(base + 0xdf7ec94);
    if (count < 1 || count > 2000000)
        return;
    cached.clear();
    nextFullScan = now() + 30000;
    for (int index = 0; index < count; ++index)
    {
        const auto object = objectAt(index);
        if (!object || (read<uint32_t>(object + 8) & 0x18000) || read<int>(object + 12) != index)
            continue;
        const auto cls = pointer(object + 0x10);
        if (!playlistClass && name(cls) == "FortPlaylistAthena")
            playlistClass = cls;
        if (!playlistEvents[0] || !playlistEvents[1] || !playlistEvents[2])
        {
            if (name(cls) == "Function")
            {
                const auto value = name(object);
                if (value == "OnPlaylistLoaded")
                    playlistEvents[0] = read<uint32_t>(object + 0x18);
                else if (value == "OnPlaylistDataLoaded")
                    playlistEvents[1] = read<uint32_t>(object + 0x18);
                else if (value == "OnRep_CurrentPlaylistInfo")
                    playlistEvents[2] = read<uint32_t>(object + 0x18);
            }
        }
        if (cls == playlistClass)
        {
            const auto kind = forPlaylist(name(object), Arenahud, competitive);
            if (kind != Kind::None && registerWidget(object, kind))
                cached.push_back({object, static_cast<uint32_t>(index), kind});
        }
    }
}
void hook(void *object, void *function, void *params)
{
    if (!busy)
    {
        const int thread = int(syscall(SYS_gettid));
        if (!gameThread.load(std::memory_order_relaxed))
        {
            const auto event = name(reinterpret_cast<uintptr_t>(function));
            if (event == "ReceiveTick" || event == "ReceiveBeginPlay")
            {
                int empty = 0;
                if (gameThread.compare_exchange_strong(empty, thread))
                    log("Game-thread lifecycle reached; scanning enabled scoring playlists.");
            }
        }
        if (gameThread.load(std::memory_order_relaxed) == thread)
        {
            const auto event = read<uint32_t>(reinterpret_cast<uintptr_t>(function) + 0x18);
            const bool loaded = event && (event == playlistEvents[0] || event == playlistEvents[1] ||
                                          event == playlistEvents[2]);
            if (loaded || now() >= nextScan)
            {
                busy = true;
                nextScan = now() + 10000;
                scan(loaded);
                busy = false;
            }
        }
    }
    original(object, function, params);
}
int locate(dl_phdr_info *info, size_t, void *)
{
    if (!info->dlpi_name || !strstr(info->dlpi_name, "/libUE4.so"))
        return 0;
    bool verified = false;
    for (int index = 0; index < info->dlpi_phnum; ++index)
    {
        const auto &ph = info->dlpi_phdr[index];
        if (ph.p_type != PT_NOTE)
            continue;
        uintptr_t cursor = info->dlpi_addr + ph.p_vaddr, end = cursor + ph.p_memsz;
        while (cursor + 12 <= end)
        {
            const auto sizes = read<Elf64_Nhdr>(cursor);
            cursor += 12;
            const auto n = cursor;
            cursor += (sizes.n_namesz + 3) & ~3u;
            const auto data = cursor;
            cursor += (sizes.n_descsz + 3) & ~3u;
            if (cursor > end)
                break;
            if (sizes.n_type == NT_GNU_BUILD_ID && sizes.n_namesz == 4 && sizes.n_descsz == sizeof(buildId) &&
                !memcmp(reinterpret_cast<void *>(n), "GNU", 4) &&
                !memcmp(reinterpret_cast<void *>(data), buildId, sizeof(buildId)))
                verified = true;
        }
    }
    if (!verified)
    {
        unsupportedBuild = true;
        log("Different libUE4 build; scoring HUD integration left disabled.");
        return 1;
    }
    base = info->dlpi_addr;
    return 1;
}
void *install(void *)
{
    for (int attempt = 0; attempt < 240 && !base && !unsupportedBuild; ++attempt)
    {
        dl_iterate_phdr(locate, nullptr);
        if (!base)
            sleep(1);
    }
    if (!base)
        return nullptr;
    constexpr unsigned char signature[] = {0xfc, 0x0f, 0x1a, 0xf8, 0xfa, 0x67, 0x01, 0xa9,
                                           0xf8, 0x5f, 0x02, 0xa9, 0xf6, 0x57, 0x03, 0xa9};
    if (memcmp(reinterpret_cast<void *>(base + 0x61b7404), signature, sizeof(signature)))
    {
        log("ProcessEvent signature mismatch; scoring HUD integration left disabled.");
        return nullptr;
    }
    original = reinterpret_cast<Event>(base + 0x61b7404);
    constexpr unsigned char converterSignature[] = {0xff, 0x83, 0x00, 0xd1, 0xfd, 0x7b, 0x01, 0xa9,
                                                    0xfd, 0x43, 0x00, 0x91, 0x08, 0x08, 0x40, 0xb9};
    constexpr unsigned char allocatorSignature[] = {0xf6, 0x57, 0xbd, 0xa9, 0xf4, 0x4f, 0x01, 0xa9,
                                                    0xfd, 0x7b, 0x02, 0xa9, 0xfd, 0x83, 0x00, 0x91};
    if (memcmp(reinterpret_cast<void *>(base + 0x62c0ec4), converterSignature, 16) ||
        memcmp(reinterpret_cast<void *>(base + 0x6264f58), allocatorSignature, 16))
    {
        log("Converter/allocator signature mismatch; no hook installed.");
        return nullptr;
    }
    // The Android actor path calls the base handler directly, so use the
    // tested ARM64 detour rather than the Windows virtual-table approach.
    const int status = DobbyHook(reinterpret_cast<void *>(base + 0x61b7404), reinterpret_cast<void *>(hook),
                                 reinterpret_cast<void **>(&original));
    __android_log_print(status == 0 ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, "AtriumHUD",
                        "Verified 11.00 ARM64 build; scoring hook result=%d (Arena=%d, Competitive=%d)",
                        status, Arenahud, competitive);
    return nullptr;
}
} // namespace
void initialize()
{
    if constexpr (!Arenahud && !competitive)
        return;
    static std::once_flag initialized;
    std::call_once(initialized,
                   []
                   {
                       pthread_t worker;
                       const int error = pthread_create(&worker, nullptr, install, nullptr);
                       if (!error)
                           pthread_detach(worker);
                       else
                           __android_log_print(ANDROID_LOG_ERROR, "AtriumHUD",
                                               "Cannot start scoring HUD initialization: %d", error);
                   });
}
} // namespace Atrium::Hud
