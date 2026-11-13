#include "stdafx.h"
#include "World/Moba/MobaBaseAura.h"

#include "Engine/Object/ZzzObject.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Terrain/ZzzLodTerrain.h"   // AddTerrainLight, RequestTerrainHeight, PrimaryTerrainLight
#include "World/MapInfra/MapManager.h"
#include "Engine/Object/ZzzCharacter.h"   // Hero

#include <algorithm>
#include <cmath>

extern double WorldTime;

// The terrain band InitTerrainLight rewrites every frame (ZzzLodTerrain.cpp).
extern int FrustrumBoundMinX;
extern int FrustrumBoundMinY;
extern int FrustrumBoundMaxX;
extern int FrustrumBoundMaxY;

namespace World::Moba::BaseAura
{

namespace
{
    struct Centre { int x; int y; };

    constexpr Centre CENTRES[] = { { 118, 13 }, { 118, 244 } };  // the blue and the red vendor
    constexpr float RADIUS_TILES = 4.f;                          // same as MobaBaseAura.RadiusTiles on the server (the base of the dome)
    constexpr float VISIBLE_TILES = 45.f;                        // nothing is spawned when the hero is farther away than this from the aura
    constexpr float FILL_PER_FRAME = 0.45f;                    // glows inside the radius per frame at the reference frame rate (scaled by the frame time)
    constexpr float RING_PER_FRAME = 0.4f;                       // glows on the edge of the radius
    constexpr float FILL_SCALE = 1.8f;
    constexpr float RING_SCALE = 1.7f;
    constexpr float TWO_PI = 6.2831853f;

    // The bunker: a translucent half sphere of light (the SD sphere model, sunk into the ground so only its top half shows) over the vendor.
    constexpr float DOME_SCALE = 2.1f;       // model scale: the dome base is about RADIUS_TILES wide at DOME_SINK
    constexpr float DOME_SINK = 233.f;       // how far the sphere centre sits below the ground (world units)
    OBJECT* s_dome[2] = { nullptr, nullptr };

    bool IsDome(const OBJECT* o)
    {
        return o != nullptr && o->Live && o->Type == MODEL_SHIELD_CRASH2 && o->SubType == DOME_SUBTYPE;
    }

    // The dome is a persistent effect without owner (see Move_MODEL_SHIELD_CRASH2, sub type DOME_SUBTYPE): made once and found again by its position.
    void EnsureDome(int index, float cx, float cy)
    {
        if (IsDome(s_dome[index]))
            return;
        s_dome[index] = nullptr;
        for (int i = 0; i < MAX_EFFECTS; ++i)
        {
            OBJECT* o = &Effects[i];
            if (IsDome(o) && fabsf(o->Position[0] - cx) < 1.f && fabsf(o->Position[1] - cy) < 1.f)
            {
                s_dome[index] = o;
                return;
            }
        }
        vec3_t position, angles, light;
        Vector(cx, cy, RequestTerrainHeight(cx, cy) - DOME_SINK, position);
        Vector(0.f, 0.f, 0.f, angles);
        Vector(0.f, 0.f, 0.f, light);
        CreateEffect(MODEL_SHIELD_CRASH2, position, angles, light, DOME_SUBTYPE, nullptr);
    }

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

    for (int index = 0; index < 2; ++index)
    {
        const Centre& centre = CENTRES[index];
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

        EnsureDome(index, cx, cy);

        for (int i = CountFor(FILL_PER_FRAME); i > 0; --i)
        {
            const float angle = static_cast<float>(rand() % 3600) * 0.1f * 3.14159265f / 180.f;
            const float distance = RADIUS_TILES * 0.75f * TERRAIN_SCALE * sqrtf(static_cast<float>(rand() % 1000) / 1000.f);
            Glow(cx + cosf(angle) * distance, cy + sinf(angle) * distance, FILL_SCALE);
        }

        for (int i = CountFor(RING_PER_FRAME); i > 0; --i)
        {
            const float angle = static_cast<float>(rand() % 3600) / 3600.f * TWO_PI;
            Glow(cx + cosf(angle) * RADIUS_TILES * 0.8f * TERRAIN_SCALE, cy + sinf(angle) * RADIUS_TILES * 0.8f * TERRAIN_SCALE, RING_SCALE);
        }
    }
}

void SnapDome(OBJECT* o)
{
    const Centre* best = &CENTRES[0];
    float bestDistance = 1e30f;
    for (const Centre& centre : CENTRES)
    {
        const float dx = o->Position[0] - (static_cast<float>(centre.x) + 0.5f) * TERRAIN_SCALE;
        const float dy = o->Position[1] - (static_cast<float>(centre.y) + 0.5f) * TERRAIN_SCALE;
        if (dx * dx + dy * dy < bestDistance)
        {
            bestDistance = dx * dx + dy * dy;
            best = &centre;
        }
    }
    const float cx = (static_cast<float>(best->x) + 0.5f) * TERRAIN_SCALE;
    const float cy = (static_cast<float>(best->y) + 0.5f) * TERRAIN_SCALE;
    Vector(cx, cy, RequestTerrainHeight(cx, cy) - DOME_SINK, o->Position);
}

float DomeScale()
{
    return DOME_SCALE;
}

void ApplyToTerrainLight()
{
    if (gMapManager.WorldActive != WD_200_MOBA_ARENA)
        return;

    // The ground glow is laid over the final light (after the hero's lamp, which sets the light around the hero to 1.0 and would hide
    // an additive tint): the red and blue channels drop toward the centre, so the green one stands out.
    const float pulse = 0.8f + 0.2f * sinf(static_cast<float>(WorldTime) * 0.004f);
    const float reach = RADIUS_TILES + 1.f;
    for (const Centre& centre : CENTRES)
    {
        const float centreX = static_cast<float>(centre.x) + 0.5f;
        const float centreY = static_cast<float>(centre.y) + 0.5f;
        if (Hero != nullptr)
        {
            const float hx = Hero->Object.Position[0] / TERRAIN_SCALE - centreX;
            const float hy = Hero->Object.Position[1] / TERRAIN_SCALE - centreY;
            if (hx * hx + hy * hy > VISIBLE_TILES * VISIBLE_TILES)
                continue;
        }

        const int firstX = std::max(std::max(FrustrumBoundMinX, 0), static_cast<int>(centreX - reach));
        const int firstY = std::max(std::max(FrustrumBoundMinY, 0), static_cast<int>(centreY - reach));
        const int lastX = std::min(std::min(FrustrumBoundMaxX + 3, 255), static_cast<int>(centreX + reach) + 1);
        const int lastY = std::min(std::min(FrustrumBoundMaxY + 3, 255), static_cast<int>(centreY + reach) + 1);
        for (int y = firstY; y <= lastY; ++y)
        {
            for (int x = firstX; x <= lastX; ++x)
            {
                const float dx = static_cast<float>(x) + 0.5f - centreX;
                const float dy = static_cast<float>(y) + 0.5f - centreY;
                const float distance = sqrtf(dx * dx + dy * dy);
                if (distance >= reach)
                    continue;

                float w = 1.f - distance / reach;
                w = w * w * (3.f - 2.f * w) * pulse;
                float* light = &PrimaryTerrainLight[TERRAIN_INDEX(x, y)][0];
                light[0] *= 1.f - 0.6f * w;
                light[1] *= 1.f + 0.1f * w;
                light[2] *= 1.f - 0.45f * w;
            }
        }
    }
}

}
