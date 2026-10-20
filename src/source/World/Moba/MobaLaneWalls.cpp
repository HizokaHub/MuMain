#include "stdafx.h"
#include "World/Moba/MobaLaneWalls.h"

#include "Render/Core/ImmediateRenderer.h" // IR::Begin / Vertex3fv
#include "Render/Terrain/ZzzLodTerrain.h"   // TerrainWall, TERRAIN_INDEX, RequestTerrainHeight
#include "Render/Textures/ZzzOpenglUtil.h"  // BindTexture, Enable/DisableAlphaBlend, depth mask, cull face
#include "Engine/Object/ZzzCharacter.h"     // Hero
#include "World/MapInfra/MapManager.h"      // gMapManager, WD_200_MOBA_ARENA

#include <algorithm>
#include <cmath>
#include <vector>

extern double WorldTime;

namespace World::Moba::LaneWalls
{

namespace
{
    constexpr int HEADER_BYTES = 5;
    constexpr int RECT_BYTES = 4;
    constexpr int GRID_SIZE = 256;

    // The server refreshes the walls every second; without news for this long they are dropped.
    constexpr double EXPIRE_MS = 3500.0;

    // The energy wall: quads of a few tiles along each rectangle, bright at the ground and fading out upward.
    constexpr int SEGMENT_TILES = 4;
    constexpr float WALL_HEIGHT = 450.f;
    constexpr float RIM_HEIGHT = 40.f;            // the brighter band at the foot of the wall
    constexpr float WALL_RED = 0.18f;
    constexpr float WALL_GREEN = 0.5f;
    constexpr float WALL_BLUE = 1.0f;
    constexpr float RIM_RED = 0.45f;
    constexpr float RIM_GREEN = 0.85f;
    constexpr float RIM_BLUE = 1.0f;
    constexpr float PULSE_BASE = 0.6f;
    constexpr float PULSE_AMOUNT = 0.4f;
    constexpr double PULSE_SPEED = 0.003;         // radians per millisecond
    constexpr float PULSE_PHASE = 0.35f;          // radians between neighbouring segments
    constexpr float TEXTURE_CENTRE = 0.5f;        // the glow texture is sampled at its middle: a flat bright colour
    constexpr float CENTRE_OFFSET = 0.5f;

    // A wall piece further than this from the hero is not drawn (in world units).
    constexpr float DRAW_RANGE = 3200.f;

    struct Rect
    {
        int x, y, w, h;
    };

    std::vector<Rect> s_rects;
    std::vector<int> s_cells;      // TERRAIN_INDEX of every wall tile
    bool s_active = false;
    double s_lastReceivedMs = 0.0;

    bool SameRects(const std::vector<Rect>& rects)
    {
        if (rects.size() != s_rects.size())
            return false;
        for (size_t i = 0; i < rects.size(); ++i)
        {
            if (rects[i].x != s_rects[i].x || rects[i].y != s_rects[i].y || rects[i].w != s_rects[i].w || rects[i].h != s_rects[i].h)
                return false;
        }
        return true;
    }

    void Drop()
    {
        // The wall flags are not taken back: the terrain is reloaded when the player leaves the arena.
        s_rects.clear();
        s_cells.clear();
        s_active = false;
    }

    void Raise(std::vector<Rect> rects)
    {
        s_cells.clear();
        for (const Rect& r : rects)
        {
            for (int x = std::max(0, r.x); x < std::min(GRID_SIZE, r.x + r.w); ++x)
            {
                for (int y = std::max(0, r.y); y < std::min(GRID_SIZE, r.y + r.h); ++y)
                    s_cells.push_back(TERRAIN_INDEX(x, y));
            }
        }
        s_rects = std::move(rects);
        s_active = true;
    }

    void KeepSolid()
    {
        for (int idx : s_cells)
            TerrainWall[idx] |= TW_NOMOVE;
    }

    void WallQuad(const float bottom0[3], const float bottom1[3], float height, const float bottomColor[3], const float topColor[3])
    {
        const float corners[4][3] = {
            { bottom0[0], bottom0[1], bottom0[2] },
            { bottom1[0], bottom1[1], bottom1[2] },
            { bottom1[0], bottom1[1], bottom1[2] + height },
            { bottom0[0], bottom0[1], bottom0[2] + height },
        };
        IR::Begin(GL_QUADS);
        for (int i = 0; i < 4; ++i)
        {
            IR::TexCoord2f(TEXTURE_CENTRE, TEXTURE_CENTRE);
            IR::Color3fv(i < 2 ? bottomColor : topColor);
            IR::Vertex3fv(corners[i]);
        }
        IR::End();
    }

    void TilePoint(float tileX, float tileY, float out[3])
    {
        out[0] = (tileX + CENTRE_OFFSET) * TERRAIN_SCALE;
        out[1] = (tileY + CENTRE_OFFSET) * TERRAIN_SCALE;
        out[2] = RequestTerrainHeight(out[0], out[1]);
    }

    void RenderSegment(const float from[3], const float to[3], int index)
    {
        const float mid[2] = { (from[0] + to[0]) * 0.5f, (from[1] + to[1]) * 0.5f };
        if (std::hypot(Hero->Object.Position[0] - mid[0], Hero->Object.Position[1] - mid[1]) > DRAW_RANGE)
            return;

        const float dark[3] = { 0.f, 0.f, 0.f };
        const float pulse = PULSE_BASE + PULSE_AMOUNT * static_cast<float>(std::sin(WorldTime * PULSE_SPEED + index * PULSE_PHASE));
        const float wall[3] = { WALL_RED * pulse, WALL_GREEN * pulse, WALL_BLUE * pulse };
        const float rim[3] = { RIM_RED * pulse, RIM_GREEN * pulse, RIM_BLUE * pulse };
        WallQuad(from, to, WALL_HEIGHT, wall, dark);
        WallQuad(from, to, RIM_HEIGHT, rim, dark);
    }

    // The wall of a rectangle is drawn along its long axis, through its middle.
    void RenderRect(const Rect& r)
    {
        const bool vertical = r.h >= r.w;
        const float fixedLine = vertical ? r.x + (r.w - 1) * 0.5f : r.y + (r.h - 1) * 0.5f;
        const int start = vertical ? r.y : r.x;
        const int length = vertical ? r.h : r.w;
        int index = 0;
        for (int offset = 0; offset < length - 1; offset += SEGMENT_TILES)
        {
            const int a = start + offset;
            const int b = std::min(start + offset + SEGMENT_TILES, start + length - 1);
            float from[3] = {};
            float to[3] = {};
            TilePoint(vertical ? fixedLine : static_cast<float>(a), vertical ? static_cast<float>(a) : fixedLine, from);
            TilePoint(vertical ? fixedLine : static_cast<float>(b), vertical ? static_cast<float>(b) : fixedLine, to);
            RenderSegment(from, to, index++);
        }
    }
}

void Receive(const unsigned char* ReceiveBuffer)
{
    const int length = ReceiveBuffer[1];
    if (length < HEADER_BYTES)
        return;

    const int count = ReceiveBuffer[4];
    if (length < HEADER_BYTES + count * RECT_BYTES)
        return;

    std::vector<Rect> rects;
    for (int i = 0; i < count; ++i)
    {
        const unsigned char* r = ReceiveBuffer + HEADER_BYTES + i * RECT_BYTES;
        rects.push_back({ r[0], r[1], r[2], r[3] });
    }

    s_lastReceivedMs = WorldTime;
    if (s_active && SameRects(rects))
        return;

    Raise(std::move(rects));
}

void Update()
{
    if (!s_active)
        return;

    if (gMapManager.WorldActive != WD_200_MOBA_ARENA || WorldTime - s_lastReceivedMs > EXPIRE_MS)
    {
        Drop();
        return;
    }

    KeepSolid();
}

void Render()
{
    if (!s_active || gMapManager.WorldActive != WD_200_MOBA_ARENA)
        return;

    EnableAlphaBlend();
    DisableCullFace();
    DisableDepthMask();
    BindTexture(BITMAP_LIGHT);
    for (const Rect& r : s_rects)
        RenderRect(r);
    EnableDepthMask();
    EnableCullFace();
    DisableAlphaBlend();
}

bool IsActive()
{
    return s_active;
}

}
