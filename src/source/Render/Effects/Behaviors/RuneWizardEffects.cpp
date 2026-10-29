#include "stdafx.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Engine/Object/ZzzObject.h"
#include "Render/Effects/ZzzEffect.h"
#include "Audio/DSPlaySound.h"
#include "MobaAegisEffect.h"
#include "RuneWizardEffects.h"

#include <algorithm>
#include <cmath>

namespace Render::Effects::RuneWizard
{
    namespace
    {
        constexpr float EffectLifeTime = 9999999.f;
        constexpr float DegreesToRadians = 3.14159265f / 180.f;

        // --- Runic Spear -----------------------------------------------------------------------------------------

        // The spear model is 760 units long with its tip 240 units below its origin.
        constexpr float SpearScale = 0.7f;
        constexpr float SpearTipOffset = 240.f * SpearScale;
        constexpr float SpearDropHeight = 650.f;

        // Ticks the spear takes to fall, and ticks it stays stuck in the ground (fading) afterwards.
        constexpr float SpearFallTicks = 9.f;
        constexpr float SpearLingerTicks = 22.f;

        // The light column grows from BeamStartScale to BeamEndScale while it fades out.
        constexpr float BeamStartScale = 2.2f;
        constexpr float BeamEndScale = 3.6f;
        constexpr float BeamTicks = 20.f;

        constexpr wchar_t SpearSoundPath[] = L"Data\\Sound\\RuneWizard\\runic_spear.wav";

        // --- Rune Phrase -----------------------------------------------------------------------------------------

        constexpr float PhraseTicks = 72.f;
        constexpr float PhraseRiseTicks = 18.f;
        constexpr float PhraseConvergeStart = 48.f;
        constexpr float PhraseConvergeEnd = 60.f;
        constexpr float PhraseFadeInTicks = 10.f;
        constexpr float PhraseFadeOutTicks = 10.f;

        constexpr int RuneCount = 5;
        constexpr float OrbitRadius = 120.f;
        constexpr float OrbitHeight = 110.f;
        constexpr float OrbitDegreesPerTick = 7.f;
        constexpr float RuneSpriteScale = 1.1f;

        // Size (in terrain tiles) of the rune circle on the ground.
        constexpr float CircleTiles = 3.4f;
        constexpr float CircleDegreesPerTick = 1.2f;

        // Colour of each rune glyph (H, M, R, T, Y).
        const float RuneColors[RuneCount][3] = {
            { 1.0f, 0.55f, 0.35f },
            { 0.45f, 0.7f, 1.0f },
            { 1.0f, 0.85f, 0.4f },
            { 0.6f, 1.0f, 0.65f },
            { 0.8f, 0.55f, 1.0f },
        };

        const int RuneTextures[RuneCount] = {
            BITMAP_RW_RUNE_H, BITMAP_RW_RUNE_M, BITMAP_RW_RUNE_R, BITMAP_RW_RUNE_T, BITMAP_RW_RUNE_Y,
        };

        // Brightness of the whole Rune Phrase over its life: fades in, holds, fades out.
        float PhraseBrightness(float age)
        {
            const float fadeIn = std::clamp(age / PhraseFadeInTicks, 0.f, 1.f);
            const float fadeOut = std::clamp((PhraseTicks - age) / PhraseFadeOutTicks, 0.f, 1.f);
            return std::min(fadeIn, fadeOut);
        }

        void SparkBurst(const float* center, int count, float spread, float r, float g, float b)
        {
            vec3_t angle, light, position;
            Vector(0.f, 0.f, 0.f, angle);
            Vector(r, g, b, light);
            for (int i = 0; i < count; ++i)
            {
                Vector(center[0] + (float)(rand() % (int)(spread * 2.f) - (int)spread),
                    center[1] + (float)(rand() % (int)(spread * 2.f) - (int)spread),
                    center[2] + (float)(rand() % 40), position);
                CreateParticle(BITMAP_SPARK + 1, position, angle, light, 11, 0.8f);
            }
        }
    }

    void CastRunicSpear(float x, float y, float groundHeight)
    {
        vec3_t position, angle, light;
        Vector(x, y, groundHeight, position);
        Vector(0.f, 0.f, 0.f, angle);
        Vector(1.f, 1.f, 1.f, light);
        CreateEffect(MODEL_RW_SPEAR, position, angle, light);
    }

    void CastRunePhrase(float x, float y, float groundHeight)
    {
        vec3_t position, angle, light;
        Vector(x, y, groundHeight, position);
        Vector(0.f, 0.f, 0.f, angle);
        Vector(1.f, 1.f, 1.f, light);
        CreateEffect(MODEL_RW_PHRASE, position, angle, light);
    }

    void InitSpear(OBJECT* o)
    {
        o->LifeTime = EffectLifeTime;
        o->Scale = SpearScale;
        o->Alpha = 0.f;
        o->LightEnable = false;
        o->Timer = 0.f;
        o->m_iAnimation = 0;
        // The spawn position is the ground point: remember it and start high above.
        o->Distance = o->Position[2];
        o->Position[2] = o->Distance + SpearTipOffset + SpearDropHeight;
        Vector(1.f, 1.f, 1.f, o->Light);
    }

    bool MoveSpear(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        o->Timer += FPS_ANIMATION_FACTOR;
        const float ground = o->Distance;

        if (o->Timer < SpearFallTicks)
        {
            const float t = o->Timer / SpearFallTicks;
            o->Position[2] = ground + SpearTipOffset + SpearDropHeight * (1.f - t * t);
            o->Alpha = std::min(1.f, t * 3.f);
            return false;
        }

        if (o->m_iAnimation == 0)
        {
            // The strike: light column, sparks and the sound.
            o->m_iAnimation = 1;
            o->Position[2] = ground + SpearTipOffset - 25.f;

            vec3_t angle, light, beamPosition;
            Vector(0.f, 0.f, 0.f, angle);
            Vector(1.f, 1.f, 1.f, light);
            Vector(o->Position[0], o->Position[1], ground + 25.f, beamPosition);
            CreateEffect(MODEL_RW_SPEAR_BEAM, beamPosition, angle, light);

            SparkBurst(beamPosition, 14, 60.f, 1.f, 0.85f, 0.5f);

            LoadWaveFile(SOUND_RW_SPEAR, SpearSoundPath, 1);
            PlayBuffer(SOUND_RW_SPEAR);
        }

        const float lingered = (o->Timer - SpearFallTicks) / SpearLingerTicks;
        if (lingered >= 1.f)
        {
            o->Live = false;
            return false;
        }

        o->Alpha = 1.f - lingered;
        return false;
    }

    void InitBeam(OBJECT* o)
    {
        o->LifeTime = EffectLifeTime;
        o->Scale = BeamStartScale;
        o->Alpha = 1.f;
        o->LightEnable = false;
        o->Timer = 0.f;
        Vector(1.f, 1.f, 1.f, o->Light);
    }

    bool MoveBeam(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        o->Timer += FPS_ANIMATION_FACTOR;
        const float t = o->Timer / BeamTicks;
        if (t >= 1.f)
        {
            o->Live = false;
            return false;
        }

        o->Scale = BeamStartScale + (BeamEndScale - BeamStartScale) * t;
        o->Alpha = 1.f - t;
        return false;
    }

    void DrawAdditive(OBJECT* o)
    {
        if (!Calc_RenderObject(o, false, 0, 0))
            return;

        // Additive blending (RENDER_BRIGHT) ignores alpha, so the fade rides on the light.
        BMD* b = &Models[o->Type];
        Vector(o->Light[0] * o->Alpha, o->Light[1] * o->Alpha, o->Light[2] * o->Alpha, b->BodyLight);
        b->RenderBody(RENDER_TEXTURE | RENDER_BRIGHT, o->Alpha, o->BlendMesh, o->BlendMeshLight, o->BlendMeshTexCoordU, o->BlendMeshTexCoordV, o->HiddenMesh);
    }

    void InitPhrase(OBJECT* o)
    {
        o->LifeTime = EffectLifeTime;
        o->Timer = 0.f;
        o->m_iAnimation = 0;
        o->Distance = o->Position[2];
        o->LightEnable = false;
        Vector(1.f, 1.f, 1.f, o->Light);
        MobaAegis::PlayCastSound();
    }

    bool MovePhrase(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        o->Timer += FPS_ANIMATION_FACTOR;
        const float age = o->Timer;
        if (age >= PhraseTicks)
        {
            o->Live = false;
            return false;
        }

        const float ground = o->Distance;
        const float brightness = PhraseBrightness(age);
        const float rise = std::clamp(age / PhraseRiseTicks, 0.f, 1.f);
        const float converge = std::clamp((age - PhraseConvergeStart) / (PhraseConvergeEnd - PhraseConvergeStart), 0.f, 1.f);
        const float radius = OrbitRadius * (1.f - converge);
        const float height = ground + 20.f + (OrbitHeight - 20.f) * rise;

        for (int i = 0; i < RuneCount; ++i)
        {
            const float angle = (age * OrbitDegreesPerTick + i * (360.f / RuneCount)) * DegreesToRadians;
            vec3_t position, light;
            Vector(o->Position[0] + cosf(angle) * radius,
                o->Position[1] + sinf(angle) * radius,
                height + sinf(age * 0.2f + i) * 12.f, position);
            Vector(RuneColors[i][0] * brightness, RuneColors[i][1] * brightness, RuneColors[i][2] * brightness, light);
            CreateSprite(RuneTextures[i], position, RuneSpriteScale, light, nullptr);
        }

        if (o->m_iAnimation == 0 && age >= PhraseConvergeEnd)
        {
            // The runes meet: a flash over the center.
            o->m_iAnimation = 1;
            vec3_t center;
            Vector(o->Position[0], o->Position[1], ground + OrbitHeight, center);
            SparkBurst(center, 16, 50.f, 0.7f, 0.8f, 1.f);
        }

        return false;
    }

    void DrawPhrase(OBJECT* o)
    {
        const float brightness = PhraseBrightness(o->Timer);
        if (brightness <= 0.f)
            return;

        vec3_t light;
        Vector(0.6f * brightness, 0.75f * brightness, 1.f * brightness, light);
        EnableAlphaBlend();
        RenderTerrainAlphaBitmap(BITMAP_RW_RUNE_CIRCLE, o->Position[0], o->Position[1], CircleTiles, CircleTiles, light, o->Timer * CircleDegreesPerTick);
        DisableAlphaBlend();
    }
}
