#pragma once

// MOBA arena markers of the corner minimap: jungle entrances, totems, fountains, the server's minimap
// feed (structures, wards, champions) and the own hero. Drawn from two atlases
// (Data\Interface\MobaPortraits.OZT and MobaMinimapIcons.OZT); every marker falls back to the old flat
// squares when its atlas did not load.

#include <utility>
#include <vector>

struct MobaMinimapEntry;

namespace UI::Minimap::MobaMarkers
{
    // Where the minimap art is on screen. Lengths are reference px along X (sx screen px each): a
    // quad w wide is drawn w * sx / sy tall so it stays square on screen.
    struct Frame
    {
        float centreX, centreY;   // map centre, reference px
        float sidePx;             // side of the map square, screen px
        float cosRot, sinRot;     // rotation the art was drawn with
        float sx, sy;             // reference -> screen px
        float markers;            // marker size factor from the options panel
        float zoom = 1.f;         // 1 = the whole map; above 1 only a window of 1/zoom of it, around (centreU, centreV)
        float centreU = 0.5f;     // window centre in texture space (u = tile Y / 256)
        float centreV = 0.5f;     // (v = tile X / 256)
        float vScale = 1.f;       // extra stretch along v (tile X): the middle-lane-only formats show a narrow strip of the map wider

        // Map-local position (0..1, texture space) to reference px, rotated like the art.
        void ToRef(float pu, float pv, float& mx, float& my) const;
        // Map tile to reference px (the art's (u, v) are (tile Y, tile X)). Returns false when the tile is
        // outside the zoom window (the marker must then not be drawn).
        bool TileToRef(float tileX, float tileY, float& mx, float& my) const;
        // A distance in map tiles, as reference px along X.
        float TilesToRef(float tiles) const;
    };

    // The two atlases (bitmap indices) and whether each one loaded.
    struct Atlases
    {
        int portraitBitmap;
        int iconBitmap;
        bool portraitsLoaded;
        bool iconsLoaded;
    };

    // Draw order, bottom to top: jungle entrances, totems, fountains, feed, own hero, totem tooltip.
    void RenderJungleEntrances(const Frame& frame, const Atlases& atlases, const std::vector<std::pair<int, int>>& doors);
    // Returns the totem under the mouse (index into g_MobaTotems), -1 = none.
    int RenderTotems(const Frame& frame, const Atlases& atlases);
    void RenderTotemTooltip(const Frame& frame, int totem);
    // The ground the camera shows, as a white outline on the minimap (the corners of the screen cast onto the ground).
    void RenderCameraView(const Frame& frame);
    void RenderFountains(const Frame& frame, const Atlases& atlases);
    // The lane minions as small dots in the colour of their team (the server sends the ones the team knows).
    void RenderCreeps(const Frame& frame, const Atlases& atlases);
    // The golden invasion: gold dots, bigger the stronger the monster (the server sends all of them to every team).
    void RenderGoldens(const Frame& frame, const Atlases& atlases);
    // The Elf's creature: a bigger gold dot (white ring while the player commands it).
    void RenderSummon(const Frame& frame, const Atlases& atlases);
    void RenderFeed(const Frame& frame, const Atlases& atlases);
    // The pings of the team: an icon (while it is fresh) and an expanding ring on the tile (World::Moba::Ping).
    void RenderPings(const Frame& frame);
    // Returns false when the portrait atlas is missing (the caller then draws the old arrow).
    bool RenderOwnHero(const Frame& frame, const Atlases& atlases);

    // Flat markers of the full (Tab) map: team colour of a feed entry and its size by kind.
    void SetEntryColor(const MobaMinimapEntry& e);
    float EntrySize(const MobaMinimapEntry& e, float champion, float structure, float nexus, float ward);
}
