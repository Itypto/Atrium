# Atrium
# Atrium Android

Atrium is a native C++ redirect designed for legacy Fortnite Android builds, inspired by [Sinum](https://github.com/projectnovafn/Sinum/tree/main/Android).
And is used for [Nextieria](https://nextieria.dev/discord).
*Note It does not contain the ch2 s1 webview redirect used for Nextieria*

Atrium features configurable backend settings and support for Arena and competitive HUDs on Android builds that do not display them natively.

## Features

- Android ARM64 support.
- Configurable backend hostname and port.
- HTTP and HTTPS support.
- Configurable TLS certificate verification.
- Arena HUD support.
- Competitive and tournament HUD support.

## Supported versions

| Fortnite version | Support |
|---|---|
| 6.21–10.30 | Supported |
| 16.00–18.40 | Supported |
| Chapter 3 | Untested |
| Other versions | Unconfirmed |

## Configuration

Atrium can be configured through `src/options/options.h`.

This file allows you to configure the backend hostname, port, networking options, Arena HUD and competitive HUD before compiling the lib.

With HTTPS and TLS verification enabled, Atrium normally retains the game's CA
bundle. If the game needs an updated PEM bundle, set the process environment
variable `ATRIUM_CA_BUNDLE` to its absolute, app-readable path **before** loading
Atrium. The bundle is used only for redirected requests; certificate and
hostname verification remain enabled. The application is responsible for
packaging and updating the bundle.

Some games install a custom curl TLS context callback that rejects redirected
servers even with a valid certificate. To use curl's normal TLS context for
redirected HTTPS requests, set `ATRIUM_USE_CURL_TLS_CONTEXT=1` before loading
Atrium. This skips the game's callback and any extra checks it performs,
including certificate pinning, for redirected requests. CA and hostname
verification remain enabled. A direct request to a non-redirected host retains
the game's callback; HTTP redirects followed by curl from a redirected request
also use curl's TLS context. Leave the variable unset unless this behavior is
needed and understood.

Arena and competitive HUD support is intended for builds where these widgets do not appear natively. If they do appear natively like in 9.10 disable competive and arena hud by setting them to **false**.

## Installation

Atrium compiles into `libAtrium.so`, an ARM64 Android shared library.

## Building Atrium

### Requirements

- Android NDK (28.2.13676358 recommended)
- CMake 3.22 or newer
- Ninja
- C++17

### Building on Windows

Open PowerShell in the Atrium project directory.

Configure your backend and HUD settings in `src/options/options.h` before building.

Set your Android NDK path:

```powershell
$env:ANDROID_NDK_HOME = "$env:LOCALAPPDATA\Android\Sdk\ndk\28.2.13676358"
```

Configure CMake:

```powershell
cmake -S . -B build -G Ninja `
  "-DCMAKE_TOOLCHAIN_FILE=$env:ANDROID_NDK_HOME\build\cmake\android.toolchain.cmake" `
  -DANDROID_ABI=arm64-v8a `
  -DANDROID_PLATFORM=android-28
```

Build Atrium:

```powershell
cmake --build build --target Atrium
```

Once successfully compiled, the library will be located at:

```
build/libAtrium.so
```

## Installation

Once you have compiled **`libAtrium.so`**, decompile Fortnite (including Smali files).

- **Step 1:** Go to `lib/arm64-v8a/`

- **Step 2:** Add `libAtrium.so`

- **Step 3:** Go to `smali/com/epicgames/ue4/GameActivity.smali`

- **Step 4:** Search for:

```smali
.method public onCreate(Landroid/os/Bundle;)V
```

- **Step 5:** Go down 1-2 lines and paste:

```smali
const-string v0, "Atrium"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
```

- **Step 6:** Save your changes and rebuild the APK.

## Troubleshooting

If Atrium fails or smth goes wrong. Diagnostic information/logs can be viewed using ADB:

```powershell
adb logcat -s Atrium
```

Ensure that your backend configuration is correct and that your backend is reachable.
## Credits

Created and maintained by Itypto.

Inspired by [Sinum](https://github.com/projectnovafn/Sinum/tree/main/Android) by Project Nova.

### Dependencies

- [Dobby](https://github.com/jmpews/Dobby) — Native function hooking.
- [libcurl](https://github.com/curl/curl) — Networking API headers.

Fortnite and Unreal Engine belong to their respective owners. Atrium is not affiliated with Epic Games.