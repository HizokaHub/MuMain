#pragma once

class OBJECT;

// Own effects of the Rune Wizard skills (imported from Season 21), in place of the S6 effects they used to borrow.
//
//  * Runic Spear: a judgement spear (Effect\judgement_spear_a01.bmd) falls from the sky onto the target place, strikes the
//    ground with a column of light (Effect\judgement_spear_a02.bmd) and a burst of sparks, then fades.
//  * Rune Phrase: a rune circle (RuneMA01_mono) glows on the ground while the five runes (H, M, R, T, Y) rise, orbit and
//    converge over the target place in a flash.
//
// Everything is visual: the damage is decided by the server.
namespace Render::Effects::RuneWizard
{
    // Casts the effect over the ground point (x, y, groundHeight) in world units.
    void CastRunicSpear(float x, float y, float groundHeight);
    void CastRunePhrase(float x, float y, float groundHeight);

    // Effect registry hooks (see EffectRegistry.cpp).
    void InitSpear(OBJECT* o);
    bool MoveSpear(OBJECT* o, int index, float luminosity);
    void InitBeam(OBJECT* o);
    bool MoveBeam(OBJECT* o, int index, float luminosity);
    void DrawAdditive(OBJECT* o);
    void InitPhrase(OBJECT* o);
    bool MovePhrase(OBJECT* o, int index, float luminosity);
    void DrawPhrase(OBJECT* o);
}
