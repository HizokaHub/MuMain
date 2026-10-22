#pragma once

#include <vector>

// MOBA minimap minions: the lane minions as small dots on the minimap. The server sends them about once a second with
// C2 hi lo D5 25  count(u16 LE)  count * (tileX tileY team)
// (the team's own minions always, the enemy ones only while the team sees them). The dots are drawn by
// UI::Minimap::MobaMarkers::RenderCreeps; this module holds the latest list.
namespace World::Moba::MinimapCreeps
{
    struct Dot
    {
        unsigned char x = 0;      // map tile X
        unsigned char y = 0;      // map tile Y
        unsigned char team = 0;   // 1 blue, 2 red
    };

    // Replaces the list with what the server sent.
    void Set(std::vector<Dot> dots);

    // The current dots; empty when the server stopped sending them (a few seconds without news).
    const std::vector<Dot>& Get();
}
