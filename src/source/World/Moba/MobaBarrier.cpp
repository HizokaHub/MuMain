#include "stdafx.h"
#include "World/Moba/MobaBarrier.h"

#include "Render/Terrain/ZzzLodTerrain.h"   // TerrainWall, TERRAIN_INDEX, RequestTerrainHeight
#include "Render/Effects/ZzzEffect.h"       // CreateParticle
#include "World/MapInfra/MapManager.h"      // gMapManager, WD_200_MOBA_ARENA
#include "Core/Utilities/Random.h"

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

    // The energy wall: sparks per frame, height of the wall in world units, and the colour (blue-white).
    constexpr int SPARKS_PER_FRAME = 10;
    constexpr float WALL_HEIGHT = 260.f;
    constexpr float SPARK_RED = 0.25f;
    constexpr float SPARK_GREEN = 0.55f;
    constexpr float SPARK_BLUE = 1.0f;
    constexpr float MIN_LUMINOSITY = 0.6f;
    constexpr float MAX_LUMINOSITY = 1.1f;
    constexpr int SPARK_SUBTYPES = 4;

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

    void SpawnSpark()
    {
        const int idx = s_ring.cells[Random::RangeInt(0, static_cast<int>(s_ring.cells.size()) - 1)];
        const int x = idx % GRID_SIZE;
        const int y = idx / GRID_SIZE;

        vec3_t position;
        position[0] = (static_cast<float>(x) + 0.5f) * TERRAIN_SCALE + Random::RangeFloat(-30.f, 30.f);
        position[1] = (static_cast<float>(y) + 0.5f) * TERRAIN_SCALE + Random::RangeFloat(-30.f, 30.f);
        position[2] = RequestTerrainHeight(position[0], position[1]) + Random::RangeFloat(0.f, WALL_HEIGHT);

        const float luminosity = Random::RangeFloat(MIN_LUMINOSITY, MAX_LUMINOSITY);
        vec3_t light;
        Vector(luminosity * SPARK_RED, luminosity * SPARK_GREEN, luminosity * SPARK_BLUE, light);
        vec3_t angle;
        Vector(0.f, 0.f, 0.f, angle);
        CreateParticle(BITMAP_FIRE, position, angle, light, Random::RangeInt(0, SPARK_SUBTYPES - 1));
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

    if (s_ring.cells.empty())
        return;

    for (int i = 0; i < SPARKS_PER_FRAME; ++i)
        SpawnSpark();
}

bool IsActive()
{
    return s_active;
}

}
