# Atrium
# Atrium Android

Atrium is a native C++ redirect designed for legacy Fortnite Android builds, inspired by [Sinum](https://github.com/projectnovafn/Sinum/tree/main/Android).

Atrium features a modular implementation, configurable backend settings and support for Arena and competitive HUDs on Android builds that do not display them natively.

## Features

- Android ARM64 support.
- Configurable backend hostname and port.
- HTTP and HTTPS support.
- Configurable TLS certificate verification.
- Arena HUD support.
- Competitive and tournament HUD support.
- Build-specific compatibility handling.
- CMake-based compilation.

## Supported versions

| Fortnite version | Support |
|---|---|
| 6.21–10.30 | Supported |
| 16.00–18.40 | Supported |
| Chapter 3 | Untested |
| Other versions | Unconfirmed |

## Differences from Sinum

While Atrium takes inspiration from Sinum, it uses a different implementation and project structure.

| Feature | Sinum | Atrium |
|---|---|---|
| Build system | Android.mk | CMake |
| Configuration | Hardcoded | options.h |
| URL handling | URI structure | Custom URL handling |
| Backend port | Hardcoded | Configurable |
| TLS verification | Disabled | Configurable |
| Arena HUD | Not included | Supported |
| Competitive HUD | Not included | Supported |
| Architecture | Multiple architectures | ARM64 |

## Configuration

Atrium can be configured through `src/options/options.h`.

This file allows you to configure the backend hostname, port, networking options, Arena HUD and competitive HUD before compiling the library.

Arena and competitive HUD support is intended for builds where these widgets do not appear natively.

## Installation

Atrium compiles into `libAtrium.so`, an ARM64 Android shared library.

Integration requires an Android client that you have permission to modify and that loads Atrium during application initialization.

Only use Atrium with authorized private backend environments. Keep TLS certificate verification enabled when connecting to public services.

## Building

Requirements:

- Android NDK
- CMake
- Ninja
- C++17-compatible compiler

## Credits

Created and maintained by Itypto.

Inspired by [Sinum](https://github.com/projectnovafn/Sinum/tree/main/Android) by Project Nova.

### Dependencies

- [Dobby](https://github.com/jmpews/Dobby) — Native function hooking.
- [libcurl](https://github.com/curl/curl) — Networking API headers.

Fortnite and Unreal Engine belong to their respective owners. Atrium is not affiliated with Epic Games.
