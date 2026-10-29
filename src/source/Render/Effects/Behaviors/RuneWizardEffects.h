#pragma once

class OBJECT;

// Own effects of the Rune Wizard skills (imported from Season 21), in place of the S6 effects they used to borrow.
// They follow what the skills look like in the game's videos.
//
//  * Runic Spear: a fiery rune sphere explodes on the target place. Three S21 models make it: a ring of rune ribbons at the
//    caster (Effect/RunicSphereCasterExternalString.bmd), a rune cage that grows over the target
//    (RunicSphereInnerSphere.bmd) and a spinning fire shell around it (RunicSphereOutSideSphere.bmd).
//  * Rune Phrase: a large crimson field spreads over the ground with a violet rune ring turning in it, while blue lances
//    (Effect/judgement_spear_a01.bmd) rain down inside it.
//
// Everything is visual: the damage is decided by the server.
namespace Render::Effects::RuneWizard
{
    // Casts the effect over the ground point (x, y, groundHeight) in world units; the Runic Spear ring follows the caster.
    void CastRunicSpear(OBJECT* caster, float x, float y, float groundHeight);
    void CastRunePhrase(float x, float y, float groundHeight);

    // Effect registry hooks (see EffectRegistry.cpp).
    void InitSphereCaster(OBJECT* o);
    bool MoveSphereCaster(OBJECT* o, int index, float luminosity);
    void InitSphereInner(OBJECT* o);
    bool MoveSphereInner(OBJECT* o, int index, float luminosity);
    void InitSphereOuter(OBJECT* o);
    bool MoveSphereOuter(OBJECT* o, int index, float luminosity);
    void InitLance(OBJECT* o);
    bool MoveLance(OBJECT* o, int index, float luminosity);
    void DrawAdditive(OBJECT* o);
    void InitField(OBJECT* o);
    bool MoveField(OBJECT* o, int index, float luminosity);
    void DrawField(OBJECT* o);
}
