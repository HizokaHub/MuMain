#pragma once

#include "World/Moba/MobaPing.h"

// The art of the pings (Data\Interface\MobaPings.OZT, tools/make_ping_atlas.py in the server repo): the ten ping icons, a ripple ring, a glow, an edge arrow,
// a disc and a border for the buttons of the wheel, and the damage wedge. Drawn square on screen whatever the shape of the window. Used by the ping HUD, the
// minimap markers and the damage HUD. Everything is in the 640x480 reference screen; angles are compass degrees (clockwise from the top).
namespace UI::Moba::PingArt
{
    // Cells of the atlas that are not a ping icon (the icon of a kind is cell kind - 1).
    constexpr int CELL_RING = 10;
    constexpr int CELL_GLOW = 11;
    constexpr int CELL_ARROW = 12;
    constexpr int CELL_DISC = 13;
    constexpr int CELL_BORDER = 14;

    struct Tint
    {
        float r, g, b;
    };

    // Loads the atlas the first time; false when the file is missing (the callers then draw nothing).
    bool Loaded();

    // The ring colour of a kind (the same as in its icon).
    Tint KindColor(World::Moba::Ping::Kind kind);

    // Switches the alpha blending on for the quads that follow, and back to normal at the end.
    void Begin();
    void End();

    // One cell centred on (cx, cy), <width> reference px wide, rotated <compass> degrees clockwise, tinted and faded.
    void DrawCell(int cell, float cx, float cy, float width, float compass = 0.f, Tint tint = { 1.f, 1.f, 1.f }, float alpha = 1.f);

    // The icon of a kind.
    void DrawKind(World::Moba::Ping::Kind kind, float cx, float cy, float width, float alpha = 1.f);

    // The damage wedge (a red arc that points up), turned toward <compass> degrees; <width> is the diameter of the circle it sits on.
    void DrawWedge(float cx, float cy, float width, float compass, Tint tint, float alpha);
}
