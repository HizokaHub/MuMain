#include "stdafx.h"
#include "World/Moba/MobaBaseAura.h"

#include "Engine/Object/ZzzObject.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Terrain/ZzzLodTerrain.h"   // AddTerrainLight, RequestTerrainHeight, PrimaryTerrainLight
#include "World/MapInfra/MapManager.h"
#include "Engine/Object/ZzzCharacter.h"   // Hero

#include <cmath>

extern double WorldTime;

namespace World::Moba::BaseAura
{

namespace
{
    struct Centre { int x; int y; };

    constexpr Centre CENTRES[] = { { 118, 13 }, { 118, 244 } };  // the blue and the red vendor
    constexpr float RADIUS_TILES = 7.f;                          // same as MobaBaseAura.RadiusTiles on the server
    constexpr float VISIBLE_TILES = 45.f;                        // nothing is spawned when the hero is farther away than this from the aura
    constexpr float FILL_PER_FRAME = 0.9f;                     // glows inside the radius per frame at the reference frame rate (scaled by the frame time)
    constexpr float RING_PER_FRAME = 1.8f;                       // glows on the edge of the radius
    constexpr float FILL_SCALE = 1.8f;
    constexpr float RING_SCALE = 1.7f;
    constexpr float TWO_PI = 6.2831853f;

    // How many particles to make this frame for a rate per frame (the whole part always, the rest by chance).
    int CountFor(float perFrame)
    {
        const float wanted = perFrame * static_cast<float>(FPS_ANIMATION_FACTOR);
        int count = static_cast<int>(wanted);
        if (static_cast<float>(rand() % 10000) / 10000.f < wanted - static_cast<float>(count))
            ++count;
        return count;
    }

    // A green glow which rises from the ground (BITMAP_LIGHT sub type 7 keeps the colour it is given and fades at the end of its life).
    void Glow(float x, float y, float scale)
    {
        vec3_t position, angles, color;
        Vector(x, y, RequestTerrainHeight(x, y) + 10.f, position);
        Vector(0.f, 0.f, 0.f, angles);
        Vector(0.25f, 1.f, 0.45f, color);
        CreateParticle(BITMAP_LIGHT, position, angles, color, 7, scale);
    }
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

        // Only while the hero is near enough to see it (the camera of the arena looks at the hero).
        if (Hero != nullptr)
        {
            const float hx = Hero->Object.Position[0] - cx;
            const float hy = Hero->Object.Position[1] - cy;
            const float visible = VISIBLE_TILES * TERRAIN_SCALE;
            if (hx * hx + hy * hy > visible * visible)
                continue;
        }

        vec3_t light;
        Vector(0.25f * pulse, 1.1f * pulse, 0.4f * pulse, light);
        AddTerrainLight(cx, cy, light, static_cast<int>(RADIUS_TILES) + 1, PrimaryTerrainLight);

        for (int i = CountFor(FILL_PER_FRAME); i > 0; --i)
        {
            const float angle = static_cast<float>(rand() % 3600) * 0.1f * 3.14159265f / 180.f;
            const float distance = RADIUS_TILES * TERRAIN_SCALE * sqrtf(static_cast<float>(rand() % 1000) / 1000.f);
            Glow(cx + cosf(angle) * distance, cy + sinf(angle) * distance, FILL_SCALE);
        }

        for (int i = CountFor(RING_PER_FRAME); i > 0; --i)
        {
            const float angle = static_cast<float>(rand() % 3600) / 3600.f * TWO_PI;
            Glow(cx + cosf(angle) * RADIUS_TILES * TERRAIN_SCALE, cy + sinf(angle) * RADIUS_TILES * TERRAIN_SCALE, RING_SCALE);
        }
    }
}

}
