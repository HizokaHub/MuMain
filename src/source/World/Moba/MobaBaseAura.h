#pragma once

// MOBA: the green aura around the blacksmith (the vendor) of each base. The server heals the champions of the team inside it by 20 % of their life, mana and
// shield every second (MobaBaseAura on the server); the client only draws it: a green light on the ground and green sparks rising inside the radius. The
// positions and the radius are the ones of the server (MobaLayout.json shops, MobaBaseAura.RadiusTiles).
namespace World::Moba::BaseAura
{
    // Called every frame: lights the ground and spawns the sparks while the hero is in the arena.
    void Update();
}
