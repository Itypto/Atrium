#pragma once

#include <string_view>

namespace Atrium::Hud
{
enum class Kind
{
    None,
    Arena,
    Competitive
};

constexpr Kind forPlaylist(std::string_view playlist, bool arena, bool competitive)
{
    constexpr std::string_view showdownPrefix = "Playlist_Showdown";
    constexpr std::string_view arenaPrefix = "Playlist_ShowdownAlt";
    if (playlist.substr(0, showdownPrefix.size()) != showdownPrefix)
        return Kind::None;
    if (playlist.substr(0, arenaPrefix.size()) == arenaPrefix)
        return arena ? Kind::Arena : Kind::None;
    return competitive ? Kind::Competitive : Kind::None;
}

constexpr const char16_t *widgetPath(Kind kind)
{
    switch (kind)
    {
    case Kind::Arena:
        return u"/Game/UI/Competitive/Arena/ArenaScoringHUD.ArenaScoringHUD_C";
    case Kind::Competitive:
        return u"/Game/UI/Frontend/Showdown/ShowdownScoringHUD.ShowdownScoringHUD_C";
    default:
        return nullptr;
    }
}
} 
