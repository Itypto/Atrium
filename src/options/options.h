#pragma once

constexpr bool Arenahud = true;
constexpr bool competitive = true;
constexpr bool enablehttps = true;
constexpr bool verifytls = true;
constexpr inline auto Backend = "api.nextieria.dev";
constexpr int port = 443;
static_assert(port > 0 && port <= 65535, "Invalid backend port");

constexpr const char *RedirectHosts[] = {
    "ol.epicgames.com",
    "on.epicgames.com",
    "akamaized.net",
};