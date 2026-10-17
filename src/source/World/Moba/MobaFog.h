#pragma once

// MOBA fog of war, visual only (step 1): what the local team cannot see is darkened and desaturated on the
// terrain (and so on the units and objects standing on it, which take their light from the terrain) and
// shaded on the corner minimap. Enemies are not hidden here; the server will do that.
//
// Mirrors the server's vision model (MobaVision): allied champions and creeps see UnitSightTiles, turrets
// and the nexus StructureSightTiles, wards WardSightTiles. Walls and solid decoration (TW_NOMOVE cells) stop the
// sight (cell-to-cell line of sight, see LineOfSight in MobaFog.cpp; the server must use the same rule); no
// terrain height and no brush yet. The sources are the ones the client knows: the own hero, the minimap feed (allied champions, structures, wards) and the
// allied characters in scope (creeps, champions nearby).
//
// Everything is a no-op outside the MOBA arena, outside a match or while the team is unknown.
namespace World::Moba::Fog
{
    // One cell per terrain tile, TERRAIN_INDEX order (y * GridSize + x).
    inline constexpr int GridSize = 256;

    // Sight radii in tiles (MobaVision.UnitRadius / StructureRadius / WardRadius on the server).
    inline constexpr float UnitSightTiles = 14.f;
    inline constexpr float StructureSightTiles = 17.f;
    inline constexpr float NexusSightTiles = StructureSightTiles * 3.f;   // the nexus sees 200 % more than a turret (MobaVision.NexusRadius)
    inline constexpr float TotemSightTiles = 14.f;   // an owned jungle totem (MobaFountains.TotemRadius)
    inline constexpr float WardSightTiles = 12.f;
    inline constexpr float FountainSightTiles = 18.f;   // an owned fountain, same as the server (MobaVision.FountainRadius)

    // How often the team's sight is recomputed, in WorldTime ms (the fog eases toward it every frame).
    inline constexpr double SightIntervalMs = 200.0;

    // Once per frame, after the entities moved: recomputes the sight at SightIntervalMs and eases the fog.
    void Update();

    // Darkens and desaturates the tiles in fog in the frustum band of PrimaryTerrainLight that
    // InitTerrainLight rewrote this frame. Must run once per InitTerrainLight, after the effect lights were
    // added and before RenderTerrain uploads the light rows.
    void ApplyToTerrainLight();

    // Whether the fog is shown this frame.
    bool IsActive();

    // Whether the tile is (almost) fully hidden from the team right now; false while the fog is off.
    bool IsTileHidden(int tileX, int tileY);

    // Whether a character that is not the hero is hidden by the fog: it is not an ally, it stands on a hidden tile and it is
    // not a structure (turrets and the nexus stay visible once the server told us where they are). tileX / tileY are the
    // character's tile, team its MobaTeam (0 neutral, 1 blue, 2 red). False while the fog is off.
    bool HidesCharacter(int team, int tileX, int tileY);

    // Eased fog of every cell, 0 (seen) .. 1 (hidden); GridSize * GridSize cells.
    const float* Cells();

    // Changes whenever Cells() does, so a consumer can cache what it derives from them.
    unsigned int Version();
}
