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

        // --- Runic Spear: the fiery rune sphere ------------------------------------------------------------------

        // Ring of rune ribbons at the caster (126 units of radius at scale 1): fades in, holds, fades out.
        constexpr float CasterTicks = 30.f;
        constexpr float CasterFadeInTicks = 4.f;
        constexpr float CasterFadeOutStart = 14.f;
        constexpr float CasterSpinDegrees = 6.f;
        constexpr float CasterHeight = 10.f;

        // Rune cage over the target (51 units of radius at scale 1): grows fast, then fades.
        constexpr float InnerTicks = 40.f;
        constexpr float InnerGrowTicks = 12.f;
        constexpr float InnerFadeOutStart = 26.f;
        constexpr float InnerStartScale = 0.8f;
        constexpr float InnerEndScale = 3.0f;
        constexpr float InnerSpinDegrees = 12.f;
        constexpr float InnerAnimationSpeed = 0.3f;
        constexpr float SphereHeight = 90.f;

        // Fire shell around it (345 units of radius at scale 1): swells while it spins, then fades.
        constexpr float OuterTicks = 36.f;
        constexpr float OuterGrowTicks = 10.f;
        constexpr float OuterFadeOutStart = 14.f;
        constexpr float OuterStartScale = 0.15f;
        constexpr float OuterEndScale = 0.7f;
        constexpr float OuterSpinDegrees = 20.f;
        constexpr float OuterAlpha = 0.85f;

        // --- Rune Phrase: the crimson field and the lances -------------------------------------------------------

        constexpr float FieldTicks = 150.f;
        constexpr float FieldGrowTicks = 18.f;
        constexpr float FieldFadeInTicks = 10.f;
        constexpr float FieldFadeOutTicks = 30.f;

        // Width (in terrain tiles) of the field at full size, and of the violet ring turning in it.
        constexpr float FieldTiles = 8.f;
        constexpr float FieldRingRatio = 0.9f;
        constexpr float FieldSpinDegrees = 0.6f;
        constexpr float FieldRingSpinDegrees = -1.5f;

        // A blue lance lands every LanceInterval ticks between LanceStart and LanceEnd, inside LanceRadius.
        constexpr float LanceStart = 6.f;
        constexpr float LanceEnd = 100.f;
        constexpr float LanceInterval = 3.f;
        constexpr int LanceRadius = 280;

        // The lance model is 760 units long with its tip 240 units below its origin.
        constexpr float LanceScale = 0.45f;
        constexpr float LanceTipOffset = 240.f * LanceScale;
        constexpr float LanceDropHeight = 500.f;
        constexpr float LanceFallTicks = 6.f;
        constexpr float LanceLingerTicks = 10.f;

        float EaseOut(float t)
        {
            const float c = std::clamp(t, 0.f, 1.f);
            return 1.f - (1.f - c) * (1.f - c);
        }

        void InitModelEffect(OBJECT* o, float scale, float alpha)
        {
            o->LifeTime = EffectLifeTime;
            o->Scale = scale;
            o->Alpha = alpha;
            o->LightEnable = false;
            o->Timer = 0.f;
            Vector(1.f, 1.f, 1.f, o->Light);
        }

        // Ages the effect by one tick; returns its age in ticks, or a negative value (and ends it) once it is older than ticks.
        float Age(OBJECT* o, float ticks)
        {
            o->Timer += FPS_ANIMATION_FACTOR;
            if (o->Timer >= ticks)
            {
                o->Live = false;
                return -1.f;
            }

            return o->Timer;
        }

        // Fades in over fadeInTicks, holds, then fades out from fadeOutStart to ticks.
        float Envelope(float age, float fadeInTicks, float fadeOutStart, float ticks)
        {
            const float fadeIn = fadeInTicks > 0.f ? std::clamp(age / fadeInTicks, 0.f, 1.f) : 1.f;
            const float fadeOut = std::clamp((ticks - age) / std::max(ticks - fadeOutStart, 1.f), 0.f, 1.f);
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

    void CastRunicSpear(OBJECT* caster, float x, float y, float groundHeight)
    {
        vec3_t position, angle, light;
        Vector(0.f, 0.f, 0.f, angle);
        Vector(1.f, 1.f, 1.f, light);

        if (caster != nullptr)
        {
            VectorCopy(caster->Position, position);
            CreateEffect(MODEL_RW_SPHERE_CASTER, position, angle, light, 0, caster);
        }

        Vector(x, y, groundHeight + SphereHeight, position);
        CreateEffect(MODEL_RW_SPHERE_INNER, position, angle, light);
        CreateEffect(MODEL_RW_SPHERE_OUTER, position, angle, light);

        vec3_t groundPosition;
        Vector(x, y, groundHeight, groundPosition);
        SparkBurst(groundPosition, 14, 60.f, 1.f, 0.7f, 0.35f);

        // The Rune Wizard's sphere sound (RunicSphere.wav) is the one the Aegis already loads.
        MobaAegis::PlayCastSound();
    }

    void CastRunePhrase(float x, float y, float groundHeight)
    {
        vec3_t position, angle, light;
        Vector(x, y, groundHeight, position);
        Vector(0.f, 0.f, 0.f, angle);
        Vector(1.f, 1.f, 1.f, light);
        CreateEffect(MODEL_RW_FIELD, position, angle, light);
    }

    void InitSphereCaster(OBJECT* o)
    {
        InitModelEffect(o, 1.f, 0.f);
    }

    bool MoveSphereCaster(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        const float age = Age(o, CasterTicks);
        if (age < 0.f)
            return false;

        if (o->Owner != nullptr && o->Owner->Live)
        {
            VectorCopy(o->Owner->Position, o->Position);
            o->Position[2] += CasterHeight;
        }

        o->Angle[2] = fmodf(o->Angle[2] + CasterSpinDegrees * FPS_ANIMATION_FACTOR, 360.f);
        o->Alpha = Envelope(age, CasterFadeInTicks, CasterFadeOutStart, CasterTicks);
        return false;
    }

    void InitSphereInner(OBJECT* o)
    {
        InitModelEffect(o, InnerStartScale, 1.f);
    }

    bool MoveSphereInner(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        const float age = Age(o, InnerTicks);
        if (age < 0.f)
            return false;

        o->Scale = InnerStartScale + (InnerEndScale - InnerStartScale) * EaseOut(age / InnerGrowTicks);
        o->Angle[2] = fmodf(o->Angle[2] + InnerSpinDegrees * FPS_ANIMATION_FACTOR, 360.f);
        o->Alpha = Envelope(age, 0.f, InnerFadeOutStart, InnerTicks);

        BMD* b = &Models[o->Type];
        b->CurrentAction = o->CurrentAction;
        b->PlayAnimation(&o->AnimationFrame, &o->PriorAnimationFrame, &o->PriorAction, InnerAnimationSpeed, o->Position, o->Angle);
        return false;
    }

    void InitSphereOuter(OBJECT* o)
    {
        InitModelEffect(o, OuterStartScale, OuterAlpha);
    }

    bool MoveSphereOuter(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        const float age = Age(o, OuterTicks);
        if (age < 0.f)
            return false;

        o->Scale = OuterStartScale + (OuterEndScale - OuterStartScale) * EaseOut(age / OuterGrowTicks);
        o->Angle[2] = fmodf(o->Angle[2] + OuterSpinDegrees * FPS_ANIMATION_FACTOR, 360.f);
        o->Alpha = OuterAlpha * Envelope(age, 0.f, OuterFadeOutStart, OuterTicks);
        return false;
    }

    void InitLance(OBJECT* o)
    {
        InitModelEffect(o, LanceScale, 0.f);
        // The spawn position is the ground point: remember it and start high above.
        o->Distance = o->Position[2];
        o->Position[2] = o->Distance + LanceTipOffset + LanceDropHeight;
        o->m_iAnimation = 0;
        Vector(0.45f, 0.65f, 1.f, o->Light);
    }

    bool MoveLance(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        const float age = Age(o, LanceFallTicks + LanceLingerTicks);
        if (age < 0.f)
            return false;

        const float ground = o->Distance;
        if (age < LanceFallTicks)
        {
            const float t = age / LanceFallTicks;
            o->Position[2] = ground + LanceTipOffset + LanceDropHeight * (1.f - t * t);
            o->Alpha = std::min(1.f, t * 3.f);
            return false;
        }

        if (o->m_iAnimation == 0)
        {
            o->m_iAnimation = 1;
            o->Position[2] = ground + LanceTipOffset - 15.f;
            vec3_t impact;
            Vector(o->Position[0], o->Position[1], ground, impact);
            SparkBurst(impact, 3, 25.f, 0.5f, 0.7f, 1.f);
        }

        o->Alpha = 1.f - (age - LanceFallTicks) / LanceLingerTicks;
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

    void InitField(OBJECT* o)
    {
        o->LifeTime = EffectLifeTime;
        o->Timer = 0.f;
        o->m_iAnimation = -1;
        o->LightEnable = false;
        Vector(1.f, 1.f, 1.f, o->Light);
        o->Distance = o->Position[2];

        vec3_t flash;
        Vector(o->Position[0], o->Position[1], o->Position[2] + 40.f, flash);
        SparkBurst(flash, 16, 80.f, 0.7f, 0.8f, 1.f);
        MobaAegis::PlayCastSound();
    }

    bool MoveField(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        const float age = Age(o, FieldTicks);
        if (age < 0.f)
            return false;

        // A new lance every LanceInterval ticks while the field is raining.
        if (age >= LanceStart && age <= LanceEnd)
        {
            const int step = (int)((age - LanceStart) / LanceInterval);
            if (step != o->m_iAnimation)
            {
                o->m_iAnimation = step;
                vec3_t position, angle, light;
                Vector(o->Position[0] + (float)(rand() % (LanceRadius * 2) - LanceRadius),
                    o->Position[1] + (float)(rand() % (LanceRadius * 2) - LanceRadius),
                    o->Distance, position);
                Vector(0.f, (float)(rand() % 20 - 10), (float)(rand() % 360), angle);
                Vector(1.f, 1.f, 1.f, light);
                CreateEffect(MODEL_RW_LANCE, position, angle, light);
            }
        }

        return false;
    }

    void DrawField(OBJECT* o)
    {
        const float age = o->Timer;
        const float brightness = Envelope(age, FieldFadeInTicks, FieldTicks - FieldFadeOutTicks, FieldTicks);
        if (brightness <= 0.f)
            return;

        const float grown = EaseOut(age / FieldGrowTicks);
        const float tiles = FieldTiles * grown;
        if (tiles <= 0.f)
            return;

        vec3_t field, ring;
        Vector(1.f * brightness, 0.15f * brightness, 0.22f * brightness, field);
        Vector(0.55f * brightness, 0.5f * brightness, 1.f * brightness, ring);

        EnableAlphaBlend();
        RenderTerrainAlphaBitmap(BITMAP_RW_FIELD, o->Position[0], o->Position[1], tiles, tiles, field, age * FieldSpinDegrees);
        RenderTerrainAlphaBitmap(BITMAP_RW_FIELD_RING, o->Position[0], o->Position[1], tiles * FieldRingRatio, tiles * FieldRingRatio, ring, age * FieldRingSpinDegrees);
        DisableAlphaBlend();
    }
}
