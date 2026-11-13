#pragma once

// MOBA: the green aura around the blacksmith (the vendor) of each base. The server heals the champions of the team inside it by 20 % of their life, mana and
// shield every second (MobaBaseAura on the server); the client only draws it: a green light on the ground and green sparks rising inside the radius. The
// positions and the radius are the ones of the server (MobaLayout.json shops, MobaBaseAura.RadiusTiles).
struct OBJECT;

namespace World::Moba::BaseAura
{
    // Sub type of MODEL_SHIELD_CRASH2 that draws the dome (handled in Move_MODEL_SHIELD_CRASH2).
    constexpr int DOME_SUBTYPE = 8;

    // Model scale of the dome.
    float DomeScale();

    // Puts a dome effect back on the ground point of the vendor it belongs to (the model animation drifts it).
    void SnapDome(OBJECT* o);

    // Called every frame: lights the ground and spawns the sparks while the hero is in the arena.
    void Update();

    // Tints the ground of the aura green. Must run after the fog / hero lamp (World::Moba::Fog::ApplyToTerrainLight), whose light would hide it,
    // and before RenderTerrain uploads the light rows.
    void ApplyToTerrainLight();
}
