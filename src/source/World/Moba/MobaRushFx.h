#pragma once

struct CHARACTER;

// MOBA Rush (skill 638, the Blade Knight closing jump brought from Season 21): what everybody sees and hears when a champion casts it.
// The server moves the champion next to its target and answers the cast with the usual skill echo; this plays the lunge pose, the sword
// flash and the two S21 sounds (Data\Sound\MobaRush\RushAttack_skill.wav when the champion leaves, RushAttackHit_skill.wav on the impact frame).
namespace World::Moba::RushFx
{
    // Plays the pose, the flash and the sound of a Rush cast by the character.
    void OnCast(CHARACTER* caster);

    // Plays the hit sound; called once on the impact frame of the lunge pose (see AttackStage in ZzzCharacter.cpp).
    void OnImpact();
}
