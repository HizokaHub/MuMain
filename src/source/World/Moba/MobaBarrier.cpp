#include "stdafx.h"
#include "World/Moba/MobaBarrier.h"

#include "Render/Core/ImmediateRenderer.h" // IR::Begin / Vertex3fv
#include "Render/Terrain/ZzzLodTerrain.h"   // TerrainWall, TERRAIN_INDEX, RequestTerrainHeight
#include "Render/Textures/ZzzOpenglUtil.h"  // BindTexture, Enable/DisableAlphaBlend, depth mask, cull face
#include "Engine/Object/ZzzCharacter.h"     // Hero
#include "World/MapInfra/MapManager.h"      // gMapManager, WD_200_MOBA_ARENA

#include <algorithm>
#include <cmath>
#include <vector>

extern double WorldTime;

namespace World::Moba::Barrier
{

namespace
{
    constexpr int PACKET_MIN_LENGTH = 10;
    constexpr int CENTER_COUNT = 2;
    constexpr int GRID_SIZE = 256;

    // Same ring as the server (MobaPreMatch.BarrierTiles): tiles from the radius up to radius + thickness.
    constexpr float RING_THICKNESS_TILES = 1.8f;

    // The server refreshes the barrier every second; without news for this long it is dropped.
    constexpr double EXPIRE_MS = 3000.0;

    // The energy wall: a ring of quads, bright at the ground and fading out upward, with a slow shimmer running around it.
    constexpr int WALL_SEGMENTS = 96;
    constexpr float WALL_HEIGHT = 600.f;
    constexpr float RIM_HEIGHT = 50.f;            // the brighter band at the foot of the wall
    constexpr float WALL_RED = 0.18f;
    constexpr float WALL_GREEN = 0.5f;
    constexpr float WALL_BLUE = 1.0f;
    constexpr float RIM_RED = 0.45f;
    constexpr float RIM_GREEN = 0.85f;
    constexpr float RIM_BLUE = 1.0f;
    constexpr float PULSE_BASE = 0.6f;
    constexpr float PULSE_AMOUNT = 0.4f;
    constexpr double PULSE_SPEED = 0.003;         // radians per millisecond
    constexpr float PULSE_PHASE = 0.25f;          // radians between neighbouring segments
    constexpr float TEXTURE_CENTRE = 0.5f;        // the glow texture is sampled at its middle: a flat bright colour
    constexpr float TWO_PI = 6.2831853f;

    // A wall further than this from the hero is not drawn (in world units; the ring has a radius of about 1100).
    constexpr float DRAW_RANGE = 3200.f;

    struct Ring
    {
        std::vector<int> cells;      // TERRAIN_INDEX of every ring cell (for the sparks)
        std::vector<int> changed;    // the cells this module turned into walls (restored on Lower)
    };

    Ring s_ring;
    bool s_active = false;
    double s_lastReceivedMs = 0.0;
    int s_radius = 0;
    int s_centers[CENTER_COUNT][2] = {};

    bool SameState(int radius, const int centers[CENTER_COUNT][2])
    {
        if (radius != s_radius)
            return false;
        for (int i = 0; i < CENTER_COUNT; ++i)
        {
            if (centers[i][0] != s_centers[i][0] || centers[i][1] != s_centers[i][1])
                return false;
        }
        return true;
    }

    void Lower()
    {
        for (int idx : s_ring.changed)
            TerrainWall[idx] &= ~TW_NOMOVE;
        s_ring.changed.clear();
        s_ring.cells.clear();
        s_active = false;
    }

    void AddRing(int cx, int cy, int radius)
    {
        const int reach = radius + 3;
        for (int x = std::max(0, cx - reach); x <= std::min(GRID_SIZE - 1, cx + reach); ++x)
        {
            for (int y = std::max(0, cy - reach); y <= std::min(GRID_SIZE - 1, cy + reach); ++y)
            {
                const float distance = std::sqrt(static_cast<float>((x - cx) * (x - cx) + (y - cy) * (y - cy)));
                if (distance < static_cast<float>(radius) || distance >= static_cast<float>(radius) + RING_THICKNESS_TILES)
                    continue;

                const int idx = TERRAIN_INDEX(x, y);
                if (TerrainWall[idx] & TW_NOMOVE)
                    continue;   // already a wall: the server has nothing to block there either

                TerrainWall[idx] |= TW_NOMOVE;
                s_ring.changed.push_back(idx);
                s_ring.cells.push_back(idx);
            }
        }
    }

    void Raise(int radius, const int centers[CENTER_COUNT][2])
    {
        Lower();
        for (int i = 0; i < CENTER_COUNT; ++i)
            AddRing(centers[i][0], centers[i][1], radius);

        s_radius = radius;
        for (int i = 0; i < CENTER_COUNT; ++i)
        {
            s_centers[i][0] = centers[i][0];
            s_centers[i][1] = centers[i][1];
        }
        s_active = true;
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

    void RenderWall(int centerX, int centerY)
    {
        const float radius = (static_cast<float>(s_radius) + RING_THICKNESS_TILES * 0.5f) * TERRAIN_SCALE;
        const float cx = (static_cast<float>(centerX) + 0.5f) * TERRAIN_SCALE;
        const float cy = (static_cast<float>(centerY) + 0.5f) * TERRAIN_SCALE;
        if (std::hypot(Hero->Object.Position[0] - cx, Hero->Object.Position[1] - cy) > DRAW_RANGE)
            return;

        const float dark[3] = { 0.f, 0.f, 0.f };
        float previous[3] = {};
        for (int i = 0; i <= WALL_SEGMENTS; ++i)
        {
            const float angle = TWO_PI * static_cast<float>(i) / static_cast<float>(WALL_SEGMENTS);
            float current[3] = { cx + radius * std::cos(angle), cy + radius * std::sin(angle), 0.f };
            current[2] = RequestTerrainHeight(current[0], current[1]);
            if (i > 0)
            {
                const float pulse = PULSE_BASE + PULSE_AMOUNT * static_cast<float>(std::sin(WorldTime * PULSE_SPEED + i * PULSE_PHASE));
                const float wall[3] = { WALL_RED * pulse, WALL_GREEN * pulse, WALL_BLUE * pulse };
                const float rim[3] = { RIM_RED * pulse, RIM_GREEN * pulse, RIM_BLUE * pulse };
                WallQuad(previous, current, WALL_HEIGHT, wall, dark);
                WallQuad(previous, current, RIM_HEIGHT, rim, dark);
            }
            memcpy(previous, current, sizeof(previous));
        }
    }
}

void Receive(const unsigned char* ReceiveBuffer)
{
    if (ReceiveBuffer[1] < PACKET_MIN_LENGTH)
        return;

    const bool active = ReceiveBuffer[4] != 0;
    if (!active)
    {
        Lower();
        return;
    }

    const int radius = ReceiveBuffer[5];
    const int centers[CENTER_COUNT][2] = { { ReceiveBuffer[6], ReceiveBuffer[7] }, { ReceiveBuffer[8], ReceiveBuffer[9] } };
    s_lastReceivedMs = WorldTime;
    if (s_active && SameState(radius, centers))
        return;

    Raise(radius, centers);
}

void Update()
{
    if (!s_active)
        return;

    if (gMapManager.WorldActive != WD_200_MOBA_ARENA || WorldTime - s_lastReceivedMs > EXPIRE_MS)
    {
        Lower();
        return;
    }

}

void Render()
{
    if (!s_active || gMapManager.WorldActive != WD_200_MOBA_ARENA)
        return;

    EnableAlphaBlend();
    DisableCullFace();
    DisableDepthMask();
    BindTexture(BITMAP_LIGHT);
    for (int i = 0; i < CENTER_COUNT; ++i)
        RenderWall(s_centers[i][0], s_centers[i][1]);
    EnableDepthMask();
    EnableCullFace();
    DisableAlphaBlend();
}

bool IsActive()
{
    return s_active;
}

}
