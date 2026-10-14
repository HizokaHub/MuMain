#include "stdafx.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Models/ZzzBMD.h"
#include "Engine/Object/ZzzObject.h"
#include "Render/Effects/ZzzEffect.h"
#include "Audio/DSPlaySound.h"
#include "World/MapInfra/MapManager.h"
#include "MobaAegisEffect.h"

#include <algorithm>
#include <cmath>

namespace Render::Effects::MobaAegis
{
    namespace
    {
        // Height of each piece above the champion's feet.
        constexpr float CageHeight = 95.f;
        constexpr float RingHeight = 6.f;

        // The cage model is ~100 units across and the ring ~252: sized to wrap a champion.
        constexpr float CageScale = 2.2f;
        constexpr float RingScale = 0.95f;

        // Animation keys per tick for the cage's own spin (its root bone turns 360 degrees over 10 keys).
        constexpr float CageSpinSpeed = 0.3f;

        // Degrees per tick the ring turns around the champion.
        constexpr float RingSpinDegrees = 1.5f;

        // Peak opacity, reached over ~10 ticks after the Aegis appears.
        constexpr float MaxAlpha = 0.8f;
        constexpr float FadeInPerTick = 0.08f;

        // The pieces live until their owner loses the Aegis (see OwnerKeepsAegis), never by lifetime.
        constexpr float EffectLifeTime = 9999999.f;

        constexpr float FullCircleDegrees = 360.f;

        constexpr wchar_t SoundPath[] = L"Data\\Sound\\MobaSD\\aegis.wav";

        // Whether the owner still wears the Aegis: in scope, alive, buffed and in the arena.
        bool OwnerKeepsAegis(OBJECT* owner)
        {
            if (owner == nullptr || !owner->Live)
                return false;
            if (owner->CurrentAction == PLAYER_DIE1 || owner->CurrentAction == PLAYER_DIE2)
                return false;
            if (gMapManager.WorldActive != WD_200_MOBA_ARENA)
                return false;
            return g_isCharacterBuff(owner, eBuff_CursedTempleProdection);
        }

        // Per-tick work shared by both pieces: removes the piece once the Aegis is gone, otherwise rides the
        // owner at the given height and fades in. Returns false when the piece was removed.
        bool FollowOwner(OBJECT* o, float height)
        {
            if (!OwnerKeepsAegis(o->Owner))
            {
                o->Live = false;
                return false;
            }

            VectorCopy(o->Owner->Position, o->Position);
            o->Position[2] += height;
            o->Alpha = std::min(o->Alpha + FadeInPerTick * FPS_ANIMATION_FACTOR, MaxAlpha);
            return true;
        }

        void PlaySpawnSound()
        {
            // LoadWaveFile is a no-op once the buffer exists, so the sound loads on the first Aegis.
            LoadWaveFile(SOUND_MOBA_AEGIS, SoundPath, 1);
            PlayBuffer(SOUND_MOBA_AEGIS);
        }
    }

    void Spawn(OBJECT* owner)
    {
        if (owner == nullptr)
            return;

        Remove(owner);

        vec3_t angle, light;
        Vector(0.f, 0.f, 0.f, angle);
        Vector(1.f, 1.f, 1.f, light);
        CreateEffect(MODEL_MOBA_AEGIS_CAGE, owner->Position, angle, light, 0, owner);
        CreateEffect(MODEL_MOBA_AEGIS_RING, owner->Position, angle, light, 0, owner);
        PlaySpawnSound();
    }

    void Remove(OBJECT* owner)
    {
        DeleteEffect(MODEL_MOBA_AEGIS_CAGE, owner);
        DeleteEffect(MODEL_MOBA_AEGIS_RING, owner);
    }

    void Init(OBJECT* o)
    {
        o->LifeTime = EffectLifeTime;
        o->Scale = (o->Type == MODEL_MOBA_AEGIS_CAGE) ? CageScale : RingScale;
        o->Alpha = 0.f;
        // A flat additive glow: no terrain light and no normal shading.
        o->LightEnable = false;
        Vector(1.f, 1.f, 1.f, o->Light);
    }

    // Both move handlers return false to skip MoveEffect's shared tail: it would stop the cage's looping
    // spin after one turn and count the lifetime down.
    bool MoveCage(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        if (!FollowOwner(o, CageHeight))
            return false;

        BMD* b = &Models[o->Type];
        b->CurrentAction = o->CurrentAction;
        b->PlayAnimation(&o->AnimationFrame, &o->PriorAnimationFrame, &o->PriorAction, CageSpinSpeed, o->Position, o->Angle);
        return false;
    }

    bool MoveRing(OBJECT* o, int /*index*/, float /*luminosity*/)
    {
        if (!FollowOwner(o, RingHeight))
            return false;

        o->Angle[2] = fmodf(o->Angle[2] + RingSpinDegrees * FPS_ANIMATION_FACTOR, FullCircleDegrees);
        return false;
    }

    void Draw(OBJECT* o)
    {
        if (!Calc_RenderObject(o, false, 0, 0))
            return;

        // Additive blending (RENDER_BRIGHT) ignores alpha, so the fade rides on the light.
        BMD* b = &Models[o->Type];
        Vector(o->Light[0] * o->Alpha, o->Light[1] * o->Alpha, o->Light[2] * o->Alpha, b->BodyLight);
        b->RenderBody(RENDER_TEXTURE | RENDER_BRIGHT, o->Alpha, o->BlendMesh, o->BlendMeshLight, o->BlendMeshTexCoordU, o->BlendMeshTexCoordV, o->HiddenMesh);
    }
}
