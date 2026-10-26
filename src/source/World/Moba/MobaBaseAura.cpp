#include "stdafx.h"
#include "World/Moba/MobaBaseAura.h"

#include "Engine/Object/ZzzObject.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Terrain/ZzzLodTerrain.h"   // AddTerrainLight, RequestTerrainHeight, PrimaryTerrainLight
#include "World/MapInfra/MapManager.h"

#include <cmath>

extern double WorldTime;

namespace World::Moba::BaseAura
{

namespace
{
    struct Centre { int x; int y; };

    constexpr Centre CENTRES[] = { { 118, 13 }, { 118, 244 } };  // the blue and the red vendor
    constexpr float RADIUS_TILES = 8.f;
    constexpr float SPARK_CHANCE_PER_FRAME = 0.9f;               // sparks per frame at the reference frame rate (scaled by the frame time)
}

void Update()
{
    if (gMapManager.WorldActive != WD_200_MOBA_ARENA)
        return;

    const float pulse = 0.8f + 0.2f * sinf(static_cast<float>(WorldTime) * 0.004f);
    for (const Centre& centre : CENTRES)
    {
        const float cx = (static_cast<float>(centre.x) + 0.5f) * TERRAIN_SCALE;
        const float cy = (static_cast<float>(centre.y) + 0.5f) * TERRAIN_SCALE;

        vec3_t light;
        Vector(0.15f * pulse, 0.9f * pulse, 0.3f * pulse, light);
        AddTerrainLight(cx, cy, light, 9, PrimaryTerrainLight);

        // Green sparks rise from the ground inside the radius.
        if (static_cast<float>(rand() % 10000) / 10000.f > SPARK_CHANCE_PER_FRAME * static_cast<float>(FPS_ANIMATION_FACTOR))
            continue;

        const float angle = static_cast<float>(rand() % 3600) * 0.1f * 3.14159265f / 180.f;
        const float distance = RADIUS_TILES * TERRAIN_SCALE * sqrtf(static_cast<float>(rand() % 1000) / 1000.f);
        vec3_t position, angles, color;
        Vector(cx + cosf(angle) * distance, cy + sinf(angle) * distance, 0.f, position);
        position[2] = RequestTerrainHeight(position[0], position[1]) + 20.f;
        Vector(0.f, 0.f, 0.f, angles);
        Vector(0.3f, 1.f, 0.4f, color);
        CreateParticle(BITMAP_SPARK, position, angles, color, 0);
    }
}

}
