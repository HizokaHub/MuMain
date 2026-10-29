#include "stdafx.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"   // CreateJoint
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

    // --- Magic Arrow, Plasma Ball, Lightning Storm, Burst and Haste ---------------------------------------------------

    namespace
    {
        constexpr float DegreesToRadians = 3.14159265f / 180.f;

        constexpr wchar_t ArrowCastSound[] = L"Data\\Sound\\RuneWizard\\magic_arrow_cast.wav";
        constexpr wchar_t ArrowHitSound[] = L"Data\\Sound\\RuneWizard\\magic_arrow_hit.wav";
        constexpr wchar_t PlasmaCastSound[] = L"Data\\Sound\\RuneWizard\\plasma_ball_cast.wav";
        constexpr wchar_t PlasmaHitSound[] = L"Data\\Sound\\RuneWizard\\plasma_ball_hit.wav";
        constexpr wchar_t StormCastSound[] = L"Data\\Sound\\RuneWizard\\lightning_storm_cast.wav";
        constexpr wchar_t StormHitSound[] = L"Data\\Sound\\RuneWizard\\lightning_storm_hit.wav";

        // Magic Arrow: flight (ticks) and the ticks the impact flash lasts.
        constexpr float ArrowMinFlightTicks = 4.f;
        constexpr float ArrowMaxFlightTicks = 12.f;
        constexpr float ArrowUnitsPerTick = 70.f;
        constexpr float ArrowImpactTicks = 7.f;
        constexpr int ArrowTrailCount = 10;
        constexpr float ArrowTrailStep = 22.f;
        constexpr float HandHeight = 110.f;
        constexpr float TargetChestHeight = 80.f;

        // Plasma Ball: three orbs, each a few ticks behind the previous.
        constexpr int OrbCount = 3;
        constexpr float OrbFlightTicks = 12.f;
        constexpr float OrbDelayTicks = 3.f;
        constexpr float OrbSideOffset = 70.f;
        constexpr float OrbImpactTicks = 6.f;
        constexpr int OrbTrailCount = 10;
        constexpr float OrbTrailStep = 20.f;

        // Lightning Storm.
        constexpr float StormTicks = 50.f;
        constexpr float StormGrowTicks = 10.f;
        constexpr float StormFadeInTicks = 8.f;
        constexpr float StormFadeOutTicks = 18.f;
        constexpr float StormTiles = 5.f;
        constexpr float StormColumnHeight = 420.f;
        constexpr int StormColumnSprites = 10;
        constexpr float StormBoltInterval = 3.f;
        constexpr float StormBoltRadius = 220.f;
        constexpr float StormBoltHeight = 520.f;

        // Burst.
        constexpr float BurstTicks = 200.f;
        constexpr float BurstRingTicks = 18.f;
        constexpr float BurstRingDelay = 6.f;
        constexpr int BurstRingCount = 3;
        constexpr float BurstPersistentStart = 20.f;
        constexpr float BurstFadeOutTicks = 25.f;
        constexpr float BurstOrbRadius = 50.f;
        constexpr float BurstOrbHeight = 115.f;

        // Haste.
        constexpr float HasteTicks = 50.f;
        constexpr int HasteRibbons = 6;
        constexpr float HasteRadius = 70.f;
        constexpr float HasteRiseHeight = 230.f;

        constexpr int StateDone = 1;

        void PlaySfx(ESound sound, const wchar_t* path)
        {
            // LoadWaveFile is a no-op once the buffer exists, so a sound loads on its first play.
            LoadWaveFile(sound, path, 1);
            PlayBuffer(sound);
        }

        // One frame of soft glow at the point: a sprite lives a single frame, so moving things re-create theirs each tick.
        void Glow(const float* point, float scale, float r, float g, float b, float brightness)
        {
            vec3_t position, light;
            VectorCopy(point, position);
            Vector(r * brightness, g * brightness, b * brightness, light);
            CreateSprite(BITMAP_SHINY + 1, position, scale, light, nullptr, 0.f);
        }

        void Lerp(const float* from, const float* to, float t, float* out)
        {
            for (int i = 0; i < 3; ++i)
                out[i] = from[i] + (to[i] - from[i]) * t;
        }

        // Unit vector from a to b (zero when they coincide).
        void Direction(const float* a, const float* b, float* out)
        {
            const float dx = b[0] - a[0];
            const float dy = b[1] - a[1];
            const float dz = b[2] - a[2];
            const float length = sqrtf(dx * dx + dy * dy + dz * dz);
            if (length < 0.001f)
            {
                Vector(0.f, 0.f, 0.f, out);
                return;
            }

            Vector(dx / length, dy / length, dz / length, out);
        }

        float Distance(const float* a, const float* b)
        {
            const float dx = b[0] - a[0];
            const float dy = b[1] - a[1];
            const float dz = b[2] - a[2];
            return sqrtf(dx * dx + dy * dy + dz * dz);
        }

        // The point where a spell leaves the caster: at hand height, a little toward the target.
        void HandPoint(OBJECT* caster, const float* toward, float* out)
        {
            VectorCopy(caster->Position, out);
            out[2] += HandHeight;
            const float dx = toward[0] - caster->Position[0];
            const float dy = toward[1] - caster->Position[1];
            const float length = sqrtf(dx * dx + dy * dy);
            if (length > 1.f)
            {
                out[0] += dx / length * 40.f;
                out[1] += dy / length * 40.f;
            }
        }

        // Ring of light on the ground around the center: additive, rotating.
        void GroundRing(int bitmap, const float* center, float tiles, float rotation, float r, float g, float b, float brightness)
        {
            if (tiles <= 0.f || brightness <= 0.f)
                return;

            vec3_t light;
            Vector(r * brightness, g * brightness, b * brightness, light);
            RenderTerrainAlphaBitmap(bitmap, center[0], center[1], tiles, tiles, light, rotation);
        }

        // Brightness of the Burst's lasting visuals: fades in after the cast rings, fades out at the end.
        float BurstBrightness(float age)
        {
            return std::min(std::clamp((age - BurstPersistentStart) / 8.f, 0.f, 1.f), std::clamp((BurstTicks - age) / BurstFadeOutTicks, 0.f, 1.f));
        }
    }

    void CastMagicArrow(OBJECT* caster, OBJECT* target)
    {
        if (caster == nullptr || target == nullptr)
            return;

        // The effect is created at the target (its Position) and InitArrow derives the start from the caster.
        vec3_t end, angle, light;
        VectorCopy(target->Position, end);
        end[2] += TargetChestHeight;
        Vector(0.f, 0.f, 0.f, angle);
        Vector(1.f, 1.f, 1.f, light);
        CreateEffect(MODEL_RW_ARROW, end, angle, light, 0, caster);
        PlaySfx(SOUND_RW_MAGIC_ARROW_CAST, ArrowCastSound);
    }

    void InitArrow(OBJECT* o)
    {
        InitModelEffect(o, 1.f, 1.f);
        o->m_iAnimation = 0;
        if (o->Owner != nullptr)
        {
            HandPoint(o->Owner, o->Position, o->StartPosition);
        }
        else
        {
            VectorCopy(o->Position, o->StartPosition);
        }

        const float flight = Distance(o->StartPosition, o->Position) / ArrowUnitsPerTick;
        o->Velocity = std::clamp(flight, ArrowMinFlightTicks, ArrowMaxFlightTicks);
    }

    bool MoveArrow(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        const float flight = o->Velocity;
        const float age = Age(o, flight + ArrowImpactTicks);
        if (age < 0.f)
            return false;

        if (age < flight)
        {
            const float t = age / flight;
            vec3_t head, dir;
            Lerp(o->StartPosition, o->Position, t * t, head);
            Direction(o->StartPosition, o->Position, dir);

            for (int k = 0; k < ArrowTrailCount; ++k)
            {
                vec3_t p;
                p[0] = head[0] - dir[0] * k * ArrowTrailStep;
                p[1] = head[1] - dir[1] * k * ArrowTrailStep;
                p[2] = head[2] - dir[2] * k * ArrowTrailStep;
                const float fade = 1.f - (float)k / ArrowTrailCount;
                Glow(p, 1.3f * fade + 0.3f, 1.f, 0.3f + 0.4f * fade, 0.1f + 0.4f * fade, fade);
            }

            Glow(head, 2.2f, 1.f, 0.85f, 0.6f, 1.f);
            return false;
        }

        if (o->m_iAnimation == 0)
        {
            o->m_iAnimation = StateDone;
            SparkBurst(o->Position, 10, 40.f, 0.6f, 0.75f, 1.f);
            SparkBurst(o->Position, 6, 30.f, 1.f, 0.6f, 0.3f);
            PlaySfx(SOUND_RW_MAGIC_ARROW_HIT, ArrowHitSound);
        }

        // Impact: a white flash with an orange halo that shrinks away.
        const float t = (age - flight) / ArrowImpactTicks;
        Glow(o->Position, 6.f * (1.f - t) + 1.f, 1.f, 0.9f, 0.75f, 1.f - t);
        Glow(o->Position, 9.f * (1.f - t) + 1.f, 1.f, 0.45f, 0.15f, 0.7f * (1.f - t));
        return false;
    }

    void CastPlasmaBall(OBJECT* caster, float x, float y, float groundHeight)
    {
        if (caster == nullptr)
            return;

        vec3_t end, angle, light;
        Vector(x, y, groundHeight + TargetChestHeight, end);
        Vector(0.f, 0.f, 0.f, angle);
        Vector(1.f, 1.f, 1.f, light);
        CreateEffect(MODEL_RW_PLASMA, end, angle, light, 0, caster);
        PlaySfx(SOUND_RW_PLASMA_BALL_CAST, PlasmaCastSound);
    }

    void InitPlasma(OBJECT* o)
    {
        InitModelEffect(o, 1.f, 1.f);
        o->m_iAnimation = 0;
        if (o->Owner != nullptr)
        {
            HandPoint(o->Owner, o->Position, o->StartPosition);
        }
        else
        {
            VectorCopy(o->Position, o->StartPosition);
        }
    }

    bool MovePlasma(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        const float total = OrbFlightTicks + OrbDelayTicks * (OrbCount - 1) + OrbImpactTicks;
        const float age = Age(o, total);
        if (age < 0.f)
            return false;

        // Sideways axis of the flight, so the orbs leave fanned out and converge on the target.
        vec3_t dir;
        Direction(o->StartPosition, o->Position, dir);
        const float sideX = -dir[1];
        const float sideY = dir[0];

        for (int i = 0; i < OrbCount; ++i)
        {
            const float local = age - OrbDelayTicks * i;
            if (local < 0.f)
                continue;

            const float side = (i - (OrbCount - 1) * 0.5f) * OrbSideOffset;
            vec3_t from;
            VectorCopy(o->StartPosition, from);
            from[0] += sideX * side;
            from[1] += sideY * side;

            if (local < OrbFlightTicks)
            {
                const float t = local / OrbFlightTicks;
                vec3_t head;
                Lerp(from, o->Position, t, head);

                for (int k = 0; k < OrbTrailCount; ++k)
                {
                    vec3_t p;
                    p[0] = head[0] - dir[0] * k * OrbTrailStep;
                    p[1] = head[1] - dir[1] * k * OrbTrailStep;
                    p[2] = head[2] - dir[2] * k * OrbTrailStep;
                    const float fade = 1.f - (float)k / OrbTrailCount;
                    Glow(p, 1.4f * fade + 0.2f, 0.8f, 0.65f, 1.f, fade * 0.8f);
                }

                Glow(head, 2.8f, 1.f, 0.8f, 0.25f, 1.f);
                Glow(head, 1.3f, 1.f, 1.f, 0.9f, 1.f);
                continue;
            }

            if (i == 0 && o->m_iAnimation == 0)
            {
                o->m_iAnimation = StateDone;
                PlaySfx(SOUND_RW_PLASMA_BALL_HIT, PlasmaHitSound);
            }

            const float impact = (local - OrbFlightTicks) / OrbImpactTicks;
            if (impact <= 1.f)
            {
                if (impact < 0.2f)
                    SparkBurst(o->Position, 4, 40.f, 1.f, 0.85f, 0.4f);

                Glow(o->Position, 5.f * (1.f - impact) + 1.f, 1.f, 0.85f, 0.45f, 1.f - impact);
            }
        }

        return false;
    }

    void CastLightningStorm(float x, float y, float groundHeight)
    {
        vec3_t position, angle, light;
        Vector(x, y, groundHeight, position);
        Vector(0.f, 0.f, 0.f, angle);
        Vector(1.f, 1.f, 1.f, light);
        CreateEffect(MODEL_RW_STORM, position, angle, light);
        PlaySfx(SOUND_RW_LIGHTNING_STORM_CAST, StormCastSound);
    }

    void InitStorm(OBJECT* o)
    {
        InitModelEffect(o, 1.f, 1.f);
        o->m_iAnimation = -1;
        o->Distance = o->Position[2];
    }

    bool MoveStorm(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        const float age = Age(o, StormTicks);
        if (age < 0.f)
            return false;

        const float brightness = Envelope(age, StormFadeInTicks, StormTicks - StormFadeOutTicks, StormTicks);

        // The column of light: a stack of white-blue glows.
        for (int i = 0; i < StormColumnSprites; ++i)
        {
            vec3_t p;
            Vector(o->Position[0], o->Position[1], o->Distance + (StormColumnHeight * i) / StormColumnSprites, p);
            Glow(p, 3.2f, 0.65f, 0.8f, 1.f, brightness * 0.8f);
        }

        // A bolt from the sky every few ticks, onto a random point of the circle.
        const int step = (int)(age / StormBoltInterval);
        if (step != o->m_iAnimation && age > StormFadeInTicks * 0.5f && age < StormTicks - StormFadeOutTicks)
        {
            const bool first = o->m_iAnimation < 0;
            o->m_iAnimation = step;
            vec3_t from, to, angle;
            const float ax = (float)(rand() % (int)(StormBoltRadius * 2.f)) - StormBoltRadius;
            const float ay = (float)(rand() % (int)(StormBoltRadius * 2.f)) - StormBoltRadius;
            Vector(o->Position[0] + ax * 0.4f, o->Position[1] + ay * 0.4f, o->Distance + StormBoltHeight, from);
            Vector(o->Position[0] + ax, o->Position[1] + ay, o->Distance, to);
            Vector(0.f, 0.f, 0.f, angle);
            CreateJoint(BITMAP_JOINT_THUNDER, from, to, angle, 4, nullptr, 8.f);
            SparkBurst(to, 3, 30.f, 0.6f, 0.75f, 1.f);

            if (first)
                PlaySfx(SOUND_RW_LIGHTNING_STORM_HIT, StormHitSound);
        }

        return false;
    }

    void DrawStorm(OBJECT* o)
    {
        const float age = o->Timer;
        const float brightness = Envelope(age, StormFadeInTicks, StormTicks - StormFadeOutTicks, StormTicks);
        const float tiles = StormTiles * EaseOut(age / StormGrowTicks);

        EnableAlphaBlend();
        GroundRing(BITMAP_RW_FIELD, o->Position, tiles, age * 0.8f, 0.4f, 0.65f, 1.f, brightness);
        GroundRing(BITMAP_RW_FIELD_RING, o->Position, tiles * 0.75f, age * -2.f, 0.8f, 0.9f, 1.f, brightness);
        DisableAlphaBlend();
    }

    void CastBurst(OBJECT* target)
    {
        if (target == nullptr)
            return;

        vec3_t angle, light;
        Vector(0.f, 0.f, 0.f, angle);
        Vector(1.f, 1.f, 1.f, light);
        CreateEffect(MODEL_RW_BURST, target->Position, angle, light, 0, target);
        MobaAegis::PlayCastSound();
    }

    void InitBurst(OBJECT* o)
    {
        InitModelEffect(o, 1.f, 1.f);
    }

    bool MoveBurst(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        const float age = Age(o, BurstTicks);
        if (age < 0.f)
            return false;

        if (o->Owner == nullptr || !o->Owner->Live)
        {
            o->Live = false;
            return false;
        }

        VectorCopy(o->Owner->Position, o->Position);

        // The violet orb that floats beside the caster while the effect lasts.
        if (age >= BurstPersistentStart)
        {
            const float brightness = BurstBrightness(age);
            const float a = age * 0.12f;
            vec3_t p;
            Vector(o->Position[0] + cosf(a) * BurstOrbRadius, o->Position[1] + sinf(a) * BurstOrbRadius, o->Position[2] + BurstOrbHeight + sinf(age * 0.1f) * 8.f, p);
            Glow(p, 1.4f, 0.7f, 0.4f, 1.f, brightness);
            Glow(p, 0.7f, 1.f, 0.9f, 1.f, brightness);
        }

        return false;
    }

    void DrawBurst(OBJECT* o)
    {
        const float age = o->Timer;

        EnableAlphaBlend();
        for (int i = 0; i < BurstRingCount; ++i)
        {
            const float t = (age - BurstRingDelay * i) / BurstRingTicks;
            if (t < 0.f || t > 1.f)
                continue;

            GroundRing(BITMAP_RW_FIELD_RING, o->Position, 1.f + 3.5f * EaseOut(t), age * (10.f + i * 4.f), 0.3f, 0.8f, 1.f, 1.f - t);
        }

        if (age >= BurstPersistentStart)
        {
            GroundRing(BITMAP_RW_FIELD_RING, o->Position, 1.3f, age * 3.f, 1.f, 0.25f, 0.15f, BurstBrightness(age) * 0.8f);
        }

        DisableAlphaBlend();
    }

    void CastHaste(OBJECT* target)
    {
        if (target == nullptr)
            return;

        vec3_t angle, light;
        Vector(0.f, 0.f, 0.f, angle);
        Vector(1.f, 1.f, 1.f, light);
        CreateEffect(MODEL_RW_HASTE, target->Position, angle, light, 0, target);
    }

    void InitHaste(OBJECT* o)
    {
        InitModelEffect(o, 1.f, 1.f);
    }

    bool MoveHaste(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        const float age = Age(o, HasteTicks);
        if (age < 0.f)
            return false;

        if (o->Owner == nullptr || !o->Owner->Live)
        {
            o->Live = false;
            return false;
        }

        VectorCopy(o->Owner->Position, o->Position);
        const float brightness = Envelope(age, 6.f, HasteTicks - 15.f, HasteTicks);

        // The amber spiral: ribbons of glow that wind up around the caster.
        for (int j = 0; j < HasteRibbons; ++j)
        {
            const float a = (age * 14.f + j * (360.f / HasteRibbons)) * DegreesToRadians;
            const float rise = fmodf(age * 8.f + j * (HasteRiseHeight / HasteRibbons), HasteRiseHeight);
            vec3_t p;
            Vector(o->Position[0] + cosf(a) * HasteRadius, o->Position[1] + sinf(a) * HasteRadius, o->Position[2] + 10.f + rise, p);
            Glow(p, 1.1f, 1.f, 0.6f, 0.15f, brightness);
        }

        return false;
    }

    void DrawHaste(OBJECT* o)
    {
        const float age = o->Timer;
        const float brightness = Envelope(age, 6.f, HasteTicks - 15.f, HasteTicks);

        EnableAlphaBlend();
        GroundRing(BITMAP_RW_FIELD_RING, o->Position, 2.4f, age * 6.f, 1.f, 0.75f, 0.2f, brightness);
        GroundRing(BITMAP_RW_FIELD, o->Position, 2.0f, age * -3.f, 1.f, 0.5f, 0.1f, brightness * 0.8f);
        DisableAlphaBlend();
    }

    void DrawNothing(OBJECT* /*o*/)
    {
    }
}
