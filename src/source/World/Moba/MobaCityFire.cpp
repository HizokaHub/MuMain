#include "stdafx.h"
#include "World/Moba/MobaCityFire.h"

#include "Engine/Object/ZzzObject.h"
#include "Render/Effects/ZzzEffect.h"
#include "World/MapInfra/MapManager.h"

extern double WorldTime;

namespace World::Moba::CityFire
{

namespace
{
    constexpr double STALE_AFTER_MS = 4000.0;     // without news for this long the fire dies out
    constexpr float FIRE_CHANCE_PER_FRAME = 0.012f; // per visible object and frame (scaled by the frame time)

    std::vector<City> s_cities;
    double s_updatedMs = -1.0e9;
}

void Set(const std::vector<City>& cities)
{
    s_cities = cities;
    s_updatedMs = WorldTime;
}

void Update()
{
    if (gMapManager.WorldActive != WD_200_MOBA_ARENA || s_cities.empty() || WorldTime - s_updatedMs > STALE_AFTER_MS)
        return;

    for (const City& city : s_cities)
    {
        if (!city.burning)
            continue;

        const int firstBlockX = std::max(0, (city.x - city.radius) / 16);
        const int lastBlockX = std::min(15, (city.x + city.radius) / 16);
        const int firstBlockY = std::max(0, (city.y - city.radius) / 16);
        const int lastBlockY = std::min(15, (city.y + city.radius) / 16);
        for (int bx = firstBlockX; bx <= lastBlockX; ++bx)
        {
            for (int by = firstBlockY; by <= lastBlockY; ++by)
            {
                OBJECT_BLOCK* block = &ObjectBlock[bx * 16 + by];
                if (!block->Visible)
                    continue;

                for (OBJECT* o = block->Head; o != nullptr; o = o->Next)
                {
                    if (!o->Live)
                        continue;

                    const float dx = o->Position[0] / TERRAIN_SCALE - static_cast<float>(city.x);
                    const float dy = o->Position[1] / TERRAIN_SCALE - static_cast<float>(city.y);
                    if (dx * dx + dy * dy > static_cast<float>(city.radius * city.radius))
                        continue;

                    if (static_cast<float>(rand() % 10000) / 10000.f > FIRE_CHANCE_PER_FRAME * static_cast<float>(FPS_ANIMATION_FACTOR))
                        continue;

                    vec3_t position, angle, light;
                    Vector(o->Position[0] + static_cast<float>(rand() % 160 - 80),
                        o->Position[1] + static_cast<float>(rand() % 160 - 80),
                        o->Position[2] + static_cast<float>(rand() % 220),
                        position);
                    Vector(0.f, 0.f, 0.f, angle);
                    Vector(1.f, 0.6f, 0.2f, light);
                    CreateParticle(BITMAP_FIRE, position, angle, light, 0, 1.6f);
                    if (rand() % 4 == 0)
                    {
                        Vector(0.6f, 0.6f, 0.6f, light);
                        CreateParticle(BITMAP_SMOKE, position, angle, light, 0, 1.2f);
                    }
                }
            }
        }
    }
}

}
