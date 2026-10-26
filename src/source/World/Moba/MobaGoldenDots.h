#pragma once

#include <vector>

// MOBA golden invasion on the HUD: the living golden monsters as gold dots on the minimap and a counter on the scoreboard. The server sends them about
// once a second while the invasion lasts with
// C2 hi lo D5 31  count(u16 LE)  count * (tileX tileY tier)
// (tier 1 small, 2 medium, 3 large, 4 Great Golden Dragon; every team gets all of them). The dots are drawn by UI::Minimap::MobaMarkers::RenderGoldens and
// the counter by the scoreboard; this module holds the latest list.
namespace World::Moba::GoldenDots
{
    struct Dot
    {
        unsigned char x = 0;      // map tile X
        unsigned char y = 0;      // map tile Y
        unsigned char tier = 1;   // 1 small .. 4 Great Golden Dragon
    };

    // How many dragons-class monsters (tier 3 and up) count as «dragons» on the counter.
    constexpr unsigned char BIG_TIER = 3;

    // Replaces the list with what the server sent.
    void Set(std::vector<Dot> dots);

    // The current dots; empty when the server stopped sending them (the invasion is over, or a few seconds without news).
    const std::vector<Dot>& Get();

    // How many of the current dots have tier BIG_TIER or more.
    int BigCount();
}
