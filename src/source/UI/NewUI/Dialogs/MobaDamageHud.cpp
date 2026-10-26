#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaDamageHud.h"

#include "Camera/CameraManager.h"              // g_Camera
#include "Camera/CameraProjection.h"           // WorldToScreen
#include "Engine/Object/ZzzCharacter.h"        // Hero
#include "Network/Server/WSclient.h"           // g_MobaLevel
#include "Render/Terrain/ZzzLodTerrain.h"      // RequestTerrainHeight, TERRAIN_SCALE
#include "UI/NewUI/Dialogs/MobaPingArt.h"
#include "UI/NewUI/NewUIManager.h"
#include "World/MapInfra/MapManager.h"         // gMapManager, WD_200_MOBA_ARENA
#include "World/Moba/MobaDamageDir.h"

#include <algorithm>
#include <cmath>

using namespace SEASON3B;
namespace Damage = World::Moba::DamageDir;
namespace Art = UI::Moba::PingArt;

extern double WorldTime;

namespace
{
    constexpr float LAYER_DEPTH = 10.1f;
    constexpr float KEY_EVENT_ORDER = 40.0f;
    constexpr DWORD UI_KEY_OFFSET = 14;

    constexpr float WEDGE_DIAMETER = 150.f;        // of the circle the wedge sits on, reference px along X
    constexpr float RING_DIAMETER = 120.f;
    constexpr float CHEST_HEIGHT = 90.f;           // world units above the feet: the wedge circles the chest of the champion
    constexpr float BASE_ALPHA = 0.40f;
    constexpr float PERCENT_ALPHA = 0.04f;         // extra alpha per percent of life lost
    constexpr double FADE_MS = 700.0;
    constexpr float PI = 3.14159265f;
    constexpr float RADIANS_TO_DEGREES = 57.29577951f;
    constexpr float PULSE_HZ = 0.012f;

    constexpr Art::Tint HEALTH_TINT = { 1.f, 0.18f, 0.12f };
    constexpr Art::Tint SHIELD_TINT = { 0.40f, 0.72f, 1.f };

    bool InArena()
    {
        return Hero != nullptr && g_MobaLevel > 0 && gMapManager.WorldActive == WD_200_MOBA_ARENA && Hero->Dead == 0;
    }

    bool Project(float x, float y, float z, float& sx, float& sy)
    {
        vec3_t position;
        position[0] = x;
        position[1] = y;
        position[2] = z;
        int px = 0, py = 0;
        CameraProjection::WorldToScreen(g_Camera, position, &px, &py);
        sx = static_cast<float>(px);
        sy = static_cast<float>(py);
        return true;
    }

    float HitAlpha(const Damage::Hit& hit)
    {
        const double left = Damage::HIT_MS - (WorldTime - hit.atMs);
        const float fade = static_cast<float>(std::clamp(left / FADE_MS, 0.0, 1.0));
        return std::clamp(BASE_ALPHA + PERCENT_ALPHA * static_cast<float>(hit.percent), 0.f, 1.f) * fade;
    }

    // The angle, clockwise from the top of the screen, from the champion to the attacker (the vertical axis is corrected so the angle is the one seen on screen).
    bool CompassToAttacker(const Damage::Hit& hit, float heroX, float heroY, float& compass)
    {
        const float worldX = (static_cast<float>(hit.tileX) + 0.5f) * TERRAIN_SCALE;
        const float worldY = (static_cast<float>(hit.tileY) + 0.5f) * TERRAIN_SCALE;
        float ax = 0.f, ay = 0.f;
        Project(worldX, worldY, RequestTerrainHeight(worldX, worldY), ax, ay);
        const float dx = ax - heroX;
        const float dy = (ay - heroY) * g_fScreenRate_y / g_fScreenRate_x;
        if (dx * dx + dy * dy < 1.f)
            return false;

        compass = std::atan2(dx, -dy) * RADIANS_TO_DEGREES;
        return true;
    }
}

void CMobaDamageHud::Create(CNewUIManager* pNewUIMng)
{
    static CMobaDamageHud s_hud;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_hud);
}

bool CMobaDamageHud::Render()
{
    if (!InArena() || !Art::Loaded())
        return true;

    const std::vector<Damage::Hit>& hits = Damage::Hits();
    if (hits.empty())
        return true;

    float heroX = 0.f, heroY = 0.f;
    Project(Hero->Object.Position[0], Hero->Object.Position[1], Hero->Object.Position[2] + CHEST_HEIGHT, heroX, heroY);

    Art::Begin();
    for (const Damage::Hit& hit : hits)
    {
        const float alpha = HitAlpha(hit);
        const Art::Tint tint = hit.shieldOnly ? SHIELD_TINT : HEALTH_TINT;
        float compass = 0.f;
        if (hit.noDirection || !CompassToAttacker(hit, heroX, heroY, compass))
        {
            const float pulse = 0.75f + 0.25f * std::sin(static_cast<float>(WorldTime) * PULSE_HZ);
            Art::DrawCell(Art::CELL_RING, heroX, heroY, RING_DIAMETER, 0.f, tint, alpha * pulse);
            continue;
        }

        Art::DrawWedge(heroX, heroY, WEDGE_DIAMETER, compass, tint, alpha);
    }
    Art::End();
    return true;
}

bool CMobaDamageHud::Update()
{
    return true;
}

bool CMobaDamageHud::UpdateMouseEvent()
{
    return true;
}

bool CMobaDamageHud::UpdateKeyEvent()
{
    return true;
}

float CMobaDamageHud::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaDamageHud::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
