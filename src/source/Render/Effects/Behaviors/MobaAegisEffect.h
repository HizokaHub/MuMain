#pragma once

class OBJECT;

// MOBA Aegis (map 200, protection buff eBuff_CursedTempleProdection): the Rune Wizard rune sphere around the
// champion. A rune cage spins over the body (Effect\rune_skillball.bmd) and a rune ring turns slowly at the
// feet (Effect\rune_skillb01.bmd), both drawn additively with their original magic_art04_sub5 texture (blue
// glyphs, orange glow). The pieces follow their champion and remove themselves once it dies, leaves the
// scope, loses the buff or the arena is left.
namespace Render::Effects::MobaAegis
{
    // Replaces any Aegis already on the owner with a fresh cage and ring and plays the Aegis sound.
    void Spawn(OBJECT* owner);

    // Removes the owner's cage and ring.
    void Remove(OBJECT* owner);

    // Effect registry hooks for MODEL_MOBA_AEGIS_CAGE / MODEL_MOBA_AEGIS_RING (see EffectRegistry.cpp).
    void Init(OBJECT* o);
    bool MoveCage(OBJECT* o, int index, float luminosity);
    bool MoveRing(OBJECT* o, int index, float luminosity);
    void Draw(OBJECT* o);
}
