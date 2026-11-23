#include "stdafx.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Models/ZzzBMD.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/AI/ZzzAI.h"
#include "Render/Effects/ZzzEffect.h"
#include "Audio/DSPlaySound.h"
#include "MobaRushFx.h"

namespace World::Moba::RushFx
{
    namespace
    {
        // The two sounds of the S21 skill (a single channel each: a second champion rushing cuts the first sound short).
        constexpr const wchar_t* AttackSoundPath = L"Data\\Sound\\MobaRush\\RushAttack_skill.wav";
        constexpr const wchar_t* HitSoundPath = L"Data\\Sound\\MobaRush\\RushAttackHit_skill.wav";
        constexpr int SoundChannels = 2;

        // SubType of MODEL_SWORD_FORCE: the plain sword flash of the Castle Siege rush.
        constexpr int SwordFlashSubType = 0;

        // LoadWaveFile is a no-op once the buffer exists, so each sound loads the first time it is played.
        void PlaySound(ESound sound, const wchar_t* path)
        {
            LoadWaveFile(sound, path, SoundChannels);
            PlayBuffer(sound);
        }

        // The hit of Lunge, the closest pose of the sword family to "rush at the target and strike" (same index math as SkillCast.cpp).
        void SetLungePose(CHARACTER* caster, OBJECT* o)
        {
            if (o->Type != MODEL_PLAYER)
                return;

            const bool mounted = caster->Helper.Type == MODEL_HORN_OF_FENRIR && !caster->SafeZone;
            const int pose = mounted ? PLAYER_FENRIR_ATTACK_MAGIC : PLAYER_ATTACK_SKILL_SWORD1 + AT_SKILL_LUNGE - AT_SKILL_FALLING_SLASH;
            SetAction(o, pose);
        }

        void CreateFlash(OBJECT* o)
        {
            vec3_t light;
            Vector(1.f, 1.f, 1.f, light);
            CreateEffect(MODEL_SWORD_FORCE, o->Position, o->Angle, light, SwordFlashSubType, o);
        }
    }

    void OnCast(CHARACTER* caster)
    {
        if (caster == nullptr)
            return;

        OBJECT* o = &caster->Object;
        SetLungePose(caster, o);
        CreateFlash(o);
        PlaySound(SOUND_MOBA_RUSH_ATTACK, AttackSoundPath);
        caster->AttackTime = 1;
    }

    void OnImpact()
    {
        PlaySound(SOUND_MOBA_RUSH_HIT, HitSoundPath);
    }
}
