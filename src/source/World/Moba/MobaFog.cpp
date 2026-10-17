#include "stdafx.h"
#include "World/Moba/MobaFog.h"

#include "Engine/Object/ZzzCharacter.h"     // Hero, CharactersClient
#include "Network/Server/WSclient.h"        // minimap feed, g_MyMobaTeam, g_MobaLevel
#include "Render/Terrain/ZzzLodTerrain.h"   // PrimaryTerrainLight, TERRAIN_INDEX, TerrainWall
#include "World/MapInfra/MapManager.h"      // gMapManager, WD_200_MOBA_ARENA

#include <algorithm>
#include <cmath>

extern double WorldTime;

// The terrain band InitTerrainLight rewrites every frame (ZzzLodTerrain.cpp).
extern int FrustrumBoundMinX;
extern int FrustrumBoundMinY;
extern int FrustrumBoundMaxX;
extern int FrustrumBoundMaxY;

namespace World::Moba::Fog
{

namespace
{
    // Off switch for testing: false leaves the arena exactly as it was without fog.
    constexpr bool FOG_ENABLED = true;

    constexpr int CELL_COUNT = GridSize * GridSize;
    constexpr float FOG_HIDDEN = 1.f;
    constexpr float FOG_SEEN = 0.f;

    // Soft sight edge, in tiles: fully seen up to (radius + EDGE_OUTSIDE - EDGE_WIDTH) from the source,
    // fully hidden from (radius + EDGE_OUTSIDE) on.
    constexpr float EDGE_OUTSIDE = 1.f;
    constexpr float EDGE_WIDTH = 2.5f;

    // A cell's fog moves toward its target at 1 / FADE_SECONDS per second, so edges fade instead of popping.
    constexpr double FADE_SECONDS = 0.25;
    constexpr double MS_PER_SECOND = 1000.0;
    // A long frame (loading, a hitch) fades at most this much at once.
    constexpr double MAX_FADE_STEP_MS = 100.0;

    // Terrain look of a tile in full fog: its light is pulled toward its own luminance tinted cold, by
    // fog * TERRAIN_FOG_STRENGTH.
    constexpr float TERRAIN_FOG_STRENGTH = 0.65f;
    constexpr float FOG_TINT_R = 0.32f;
    constexpr float FOG_TINT_G = 0.35f;
    constexpr float FOG_TINT_B = 0.42f;
    constexpr float LUMA_R = 0.299f;
    constexpr float LUMA_G = 0.587f;
    constexpr float LUMA_B = 0.114f;

    // InitTerrainLight rewrites the light up to FrustrumBoundMax + 3 on each axis.
    constexpr int TERRAIN_BAND_EXTRA = 3;

    // The hero's own light: the terrain around the hero is fully lit (light 1.0) up to HERO_LIGHT_FULL_TILES, then falls
    // off smoothly to the normal light at HERO_LIGHT_FULL_TILES + HERO_LIGHT_FADE_TILES (about the edge of the screen).
    // It is laid under the fog, so what the team cannot see still stays dark.
    constexpr float HERO_LIGHT_FULL_TILES = 40.f;
    constexpr float HERO_LIGHT_FADE_TILES = 10.f;

    // The kind byte of the minimap feed (MobaMinimapEntry::kind).
    constexpr BYTE FEED_CHAMPION = 1;
    constexpr BYTE FEED_TURRET = 2;
    constexpr BYTE FEED_NEXUS = 3;
    constexpr BYTE FEED_WARD = 4;

    // Cells and feed entries are tile indices; a tile's centre is half a tile in.
    constexpr float TILE_CENTRE = 0.5f;

    float s_targetFog[CELL_COUNT];   // fog the team's sight asks for, rebuilt every SightIntervalMs
    float s_fog[CELL_COUNT];         // what is shown, eased toward s_targetFog
    bool s_active = false;
    bool s_settled = true;           // s_fog == s_targetFog everywhere: nothing to ease
    double s_lastSightTime = 0.0;
    double s_lastFrameTime = 0.0;
    unsigned int s_version = 0;

    bool ShouldBeActive()
    {
        return FOG_ENABLED
            && gMapManager.WorldActive == WD_200_MOBA_ARENA
            && g_MobaLevel > 0
            && g_MyMobaTeam != 0
            && Hero != nullptr;
    }

    // Walls and solid decoration (the cells nobody can walk through) stop the sight.
    bool Opaque(int x, int y)
    {
        return (TerrainWall[TERRAIN_INDEX(x, y)] & TW_NOMOVE) != 0;
    }

    // Whether the cell (toX, toY) is seen from the cell (fromX, fromY): the cells the straight line between their
    // centres crosses must not be opaque. A target that is itself opaque is seen (its face), what lies behind it
    // is not. A source standing inside opaque cells (a tower, the nexus) sees out of them: opaque cells before the
    // first open one do not count. The line goes through the exact corner between two cells only if they are not
    // both opaque. Integer arithmetic only, so the server (MobaSight) gets the very same result.
    bool LineOfSight(int fromX, int fromY, int toX, int toY)
    {
        const int dx = toX - fromX;
        const int dy = toY - fromY;
        const int nx = dx < 0 ? -dx : dx;
        const int ny = dy < 0 ? -dy : dy;
        const int stepX = dx < 0 ? -1 : 1;
        const int stepY = dy < 0 ? -1 : 1;

        bool inside = Opaque(fromX, fromY);
        int x = fromX;
        int y = fromY;
        int ix = 0;
        int iy = 0;
        while (ix < nx || iy < ny)
        {
            const int decision = (1 + 2 * ix) * ny - (1 + 2 * iy) * nx;
            if (decision == 0)
            {
                if (!inside && Opaque(x + stepX, y) && Opaque(x, y + stepY))
                    return false;
                x += stepX;
                y += stepY;
                ++ix;
                ++iy;
            }
            else if (decision < 0)
            {
                x += stepX;
                ++ix;
            }
            else
            {
                y += stepY;
                ++iy;
            }

            if (x == toX && y == toY)
                return true;

            if (Opaque(x, y))
            {
                if (!inside)
                    return false;
            }
            else
            {
                inside = false;
            }
        }
        return true;
    }

    // Lifts the fog in a disc around (centreX, centreY), in tiles, keeping the lowest fog of each cell. Only the
    // cells the centre has a line of sight to are lifted.
    void RevealDisc(float centreX, float centreY, float radius)
    {
        const int sourceX = std::clamp((int)floorf(centreX), 0, GridSize - 1);
        const int sourceY = std::clamp((int)floorf(centreY), 0, GridSize - 1);
        const float outer = radius + EDGE_OUTSIDE;
        const float inner = std::max(outer - EDGE_WIDTH, 0.f);
        const float outerSq = outer * outer;
        const float innerSq = inner * inner;

        const int firstX = std::max(0, (int)floorf(centreX - outer));
        const int lastX = std::min(GridSize - 1, (int)ceilf(centreX + outer));
        const int firstY = std::max(0, (int)floorf(centreY - outer));
        const int lastY = std::min(GridSize - 1, (int)ceilf(centreY + outer));

        for (int y = firstY; y <= lastY; ++y)
        {
            const float dy = (float)y + TILE_CENTRE - centreY;
            for (int x = firstX; x <= lastX; ++x)
            {
                const float dx = (float)x + TILE_CENTRE - centreX;
                const float distanceSq = dx * dx + dy * dy;
                if (distanceSq >= outerSq)
                    continue;

                if (!LineOfSight(sourceX, sourceY, x, y))
                    continue;

                const float fog = (distanceSq <= innerSq) ? FOG_SEEN : (sqrtf(distanceSq) - inner) / EDGE_WIDTH;
                float& cell = s_targetFog[y * GridSize + x];
                if (fog < cell)
                    cell = fog;
            }
        }
    }

    float FeedSightTiles(const MobaMinimapEntry& e)
    {
        switch (e.kind)
        {
        case FEED_CHAMPION:
            return e.hp > 0 ? UnitSightTiles : 0.f;   // a dead ally (hp 0) sees nothing
        case FEED_TURRET:
            return StructureSightTiles;
        case FEED_NEXUS:
            return NexusSightTiles;
        case FEED_WARD:
            return WardSightTiles;
        default:
            return 0.f;
        }
    }

    void AddHeroSight()
    {
        if (Hero->Dead > 0.f)
            return;

        const OBJECT& o = Hero->Object;
        RevealDisc(o.Position[0] / TERRAIN_SCALE, o.Position[1] / TERRAIN_SCALE, UnitSightTiles);
    }

    // Allied champions anywhere on the map, allied structures and wards.
    void AddFeedSight()
    {
        for (int m = 0; m < g_MobaMinimapCount; ++m)
        {
            const MobaMinimapEntry& e = g_MobaMinimap[m];
            if (e.team != g_MyMobaTeam)
                continue;

            const float radius = FeedSightTiles(e);
            if (radius > 0.f)
                RevealDisc((float)e.x + TILE_CENTRE, (float)e.y + TILE_CENTRE, radius);
        }
    }

    // Allied creeps and champions in scope (team from packet C1 D5 01), at their exact positions.
    void AddScopeSight()
    {
        if (CharactersClient == nullptr)
            return;

        for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
        {
            const CHARACTER& c = CharactersClient[i];
            const OBJECT& o = c.Object;
            if (!o.Live || c.Dead > 0.f || c.MobaTeam != g_MyMobaTeam)
                continue;
            if (o.Kind != KIND_MONSTER && o.Kind != KIND_PLAYER)
                continue;

            RevealDisc(o.Position[0] / TERRAIN_SCALE, o.Position[1] / TERRAIN_SCALE, UnitSightTiles);
        }
    }

    // A fountain a team owns gives that team vision around it (server: MobaVision.FountainRadius).
    void AddFountainSight()
    {
        for (int i = 0; i < MOBA_FOUNTAIN_COUNT; ++i)
        {
            if (g_MobaFountainInfo[i].owner == g_MyMobaTeam)
                RevealDisc((float)g_MobaFountains[i][0] + TILE_CENTRE, (float)g_MobaFountains[i][1] + TILE_CENTRE, FountainSightTiles);
        }
    }

    // A jungle totem a team owns gives that team vision around it (server: MobaFountains.TotemRadius).
    void AddTotemSight()
    {
        for (int k = 0; k < MOBA_TOTEM_COUNT; ++k)
        {
            if (g_MobaFountainInfo[MOBA_FOUNTAIN_COUNT + k].owner == g_MyMobaTeam)
                RevealDisc((float)g_MobaTotems[k][0] + TILE_CENTRE, (float)g_MobaTotems[k][1] + TILE_CENTRE, TotemSightTiles);
        }
    }

    void RebuildSight()
    {
        std::fill(s_targetFog, s_targetFog + CELL_COUNT, FOG_HIDDEN);
        AddHeroSight();
        AddFeedSight();
        AddFountainSight();
        AddTotemSight();
        AddScopeSight();
        s_settled = false;
    }

    // Moves every cell toward its target by the fade of elapsedMs.
    void EaseFog(double elapsedMs)
    {
        if (s_settled)
            return;

        const double clampedMs = std::clamp(elapsedMs, 0.0, MAX_FADE_STEP_MS);
        const float step = (float)(clampedMs / (FADE_SECONDS * MS_PER_SECOND));
        if (step <= 0.f)
            return;

        bool changed = false;
        bool settled = true;
        for (int i = 0; i < CELL_COUNT; ++i)
        {
            const float target = s_targetFog[i];
            float fog = s_fog[i];
            if (fog == target)
                continue;

            fog = (fog < target) ? std::min(fog + step, target) : std::max(fog - step, target);
            s_fog[i] = fog;
            changed = true;
            if (fog != target)
                settled = false;
        }

        s_settled = settled;
        if (changed)
            ++s_version;
    }

    // Entering a match shows the fog at once instead of fading the whole map in.
    void SnapFog()
    {
        std::copy(s_targetFog, s_targetFog + CELL_COUNT, s_fog);
        s_settled = true;
        ++s_version;
    }

    void FogTerrainVertex(float* light, float amount)
    {
        const float luminance = light[0] * LUMA_R + light[1] * LUMA_G + light[2] * LUMA_B;
        light[0] += (luminance * FOG_TINT_R - light[0]) * amount;
        light[1] += (luminance * FOG_TINT_G - light[1]) * amount;
        light[2] += (luminance * FOG_TINT_B - light[2]) * amount;
    }
}

void Update()
{
    if (!ShouldBeActive())
    {
        s_active = false;
        return;
    }

    const double now = WorldTime;
    const bool activating = !s_active;
    s_active = true;

    // now < last: WorldTime restarted (reconnect).
    if (activating || now - s_lastSightTime >= SightIntervalMs || now < s_lastSightTime)
    {
        RebuildSight();
        s_lastSightTime = now;
    }

    if (activating)
        SnapFog();
    else
        EaseFog(now - s_lastFrameTime);
    s_lastFrameTime = now;
}

void ApplyToTerrainLight()
{
    if (!s_active)
        return;

    // The same band InitTerrainLight rewrote (and marked dirty) this frame, so no extra row gets dirty and
    // the darkening never stacks on last frame's.
    const int firstX = std::max(FrustrumBoundMinX, 0);
    const int firstY = std::max(FrustrumBoundMinY, 0);
    const int lastX = std::min(FrustrumBoundMaxX + TERRAIN_BAND_EXTRA, GridSize - 1);
    const int lastY = std::min(FrustrumBoundMaxY + TERRAIN_BAND_EXTRA, GridSize - 1);

    const float heroX = Hero != nullptr ? Hero->Object.Position[0] / TERRAIN_SCALE : -1000.f;
    const float heroY = Hero != nullptr ? Hero->Object.Position[1] / TERRAIN_SCALE : -1000.f;
    constexpr float lampEnd = HERO_LIGHT_FULL_TILES + HERO_LIGHT_FADE_TILES;

    for (int y = firstY; y <= lastY; ++y)
    {
        for (int x = firstX; x <= lastX; ++x)
        {
            const int index = TERRAIN_INDEX(x, y);

            const float lampDx = (float)x + TILE_CENTRE - heroX;
            const float lampDy = (float)y + TILE_CENTRE - heroY;
            const float lampDistance = sqrtf(lampDx * lampDx + lampDy * lampDy);

            // The hero's lamp, and the lamp of the whole team: what any ally, turret or ward sees is lit like what the hero sees
            // (the camera can look at an ally's surroundings, which have to look as they do for him).
            float t = 0.f;
            if (lampDistance < lampEnd)
            {
                t = (lampDistance <= HERO_LIGHT_FULL_TILES) ? 1.f : 1.f - (lampDistance - HERO_LIGHT_FULL_TILES) / HERO_LIGHT_FADE_TILES;
                t = t * t * (3.f - 2.f * t);
            }
            const float seen = 1.f - s_fog[index];
            if (seen > t)
                t = seen;
            if (t > 0.f)
            {
                float* light = &PrimaryTerrainLight[index][0];
                for (int c = 0; c < 3; ++c)
                    light[c] += (1.f - light[c]) * t;
            }

            const float amount = s_fog[index] * TERRAIN_FOG_STRENGTH;
            if (amount > 0.f)
                FogTerrainVertex(&PrimaryTerrainLight[index][0], amount);
        }
    }
}

bool IsActive()
{
    return s_active;
}

bool IsTileHidden(int tileX, int tileY)
{
    if (!s_active || tileX < 0 || tileY < 0 || tileX >= GridSize || tileY >= GridSize)
        return false;
    return s_fog[tileY * GridSize + tileX] > 0.85f;
}

bool HidesCharacter(int team, int tileX, int tileY)
{
    if (!s_active || team == g_MyMobaTeam || !IsTileHidden(tileX, tileY))
        return false;

    // A structure the team has been told about (the minimap feed lists turrets and nexuses of both teams) is not hidden.
    for (int m = 0; m < g_MobaMinimapCount; ++m)
    {
        const MobaMinimapEntry& e = g_MobaMinimap[m];
        if ((e.kind == FEED_TURRET || e.kind == FEED_NEXUS) && std::abs((int)e.x - tileX) <= 2 && std::abs((int)e.y - tileY) <= 2)
            return false;
    }
    return true;
}

const float* Cells()
{
    return s_fog;
}

unsigned int Version()
{
    return s_version;
}

}
