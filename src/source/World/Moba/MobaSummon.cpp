#include "stdafx.h"
#include "World/Moba/MobaSummon.h"

#include "Network/Server/WSclient.h"  // SendMobaSummonControl, SendMobaSummonOrder, SendMobaSummonSkill
#include "Engine/Object/ZzzInterface.h"   // MobaSummonCastAtCursor
#include "UI/NewUI/NewUICommon.h"          // IsPress

#include <algorithm>
#include <iterator>

extern double WorldTime;

namespace World::Moba::Summon
{

namespace
{
    // Without news for this long the creature is taken as gone (the match ended or the packets stopped).
    constexpr double STALE_AFTER_MS = 4000.0;

    constexpr float SIZE_PER_LEVEL = 0.075f;   // +7.5 % per level: 1.68x at level 10

    // Skills by appearance (30 goblin, 31 stone golem, 32 assassin, 33 elite yeti, 34 dark knight, 35 bali, 36 soldier); the icons are the ones of the client skills
    // whose animation the server shows. The cooldowns and ranks are the ones of the server (MobaSummonSkills).
    constexpr SkillInfo SKILLS[7][2] = {
        { { L"Rapidez", 28, 14, 1 }, {} },
        { { L"Golpe s\u00edsmico", 62, 9, 1 }, { L"Piel de piedra", 27, 20, 3 } },
        { { L"Pu\u00f1alada", 43, 7, 1 }, { L"Salto", 6, 10, 3 } },
        { { L"Aliento helado", 7, 10, 1 }, { L"Piel de escarcha", 26, 18, 3 } },
        { { L"Tajo giratorio", 22, 7, 1 }, { L"Grito de guerra", 64, 16, 3 } },
        { { L"Bola de fuego", 4, 5, 1 }, {} },
        { { L"Disparo m\u00faltiple", 24, 7, 1 }, {} },
    };
    constexpr int FIRST_APPEARANCE = 30;
    constexpr int APPEARANCE_COUNT = 7;
    constexpr double MS_PER_TENTH = 100.0;

    double s_skillReadyMs[2] = {};
    State s_state;
    std::vector<std::pair<unsigned short, int>> s_sizes;
    std::vector<std::pair<unsigned short, int>> s_growth;
    double s_updatedMs = -1.0e9;
    bool s_localControl = false;   // set at once by the key, until the server confirms
}

void Set(State state)
{
    s_state = state;
    s_localControl = state.present && state.control;
    s_updatedMs = WorldTime;
}

int SkillCount()
{
    const int index = Get().appearance - FIRST_APPEARANCE;
    if (!Get().present || index < 0 || index >= APPEARANCE_COUNT)
        return 0;

    return SKILLS[index][1].name != nullptr ? 2 : 1;
}

const SkillInfo& Skill(int slot)
{
    return SKILLS[std::clamp(Get().appearance - FIRST_APPEARANCE, 0, APPEARANCE_COUNT - 1)][std::clamp(slot, 0, 1)];
}

double SkillRemainingMs(int slot)
{
    return std::max(0.0, s_skillReadyMs[std::clamp(slot, 0, 1)] - WorldTime);
}

void SetCooldowns(int firstTenths, int secondTenths)
{
    const int tenths[2] = { firstTenths, secondTenths };
    for (int slot = 0; slot < 2; ++slot)
    {
        // The server is the one who knows (a skill used by the creature itself, or a cast which was refused).
        s_skillReadyMs[slot] = WorldTime + tenths[slot] * MS_PER_TENTH;
    }
}

bool UpdateKeys()
{
    if (!Controlling())
        return false;

    for (int slot = 0; slot < SkillCount(); ++slot)
    {
        if (SEASON3B::IsPress('1' + slot))
        {
            MobaSummonCastAtCursor(slot);
            return true;
        }
    }
    return false;
}

void Cast(int slot, unsigned short key)
{
    if (!Controlling() || slot < 0 || slot >= SkillCount())
        return;

    SendMobaSummonSkill(static_cast<unsigned char>(slot), key);
    // The cooldown is shown at once; the next packet of the server corrects it (a cast out of rank or on cooldown is refused there).
    if (SkillRemainingMs(slot) <= 0.0 && Get().rank >= Skill(slot).minRank)
        s_skillReadyMs[slot] = WorldTime + Skill(slot).cooldownSeconds * 1000.0;
}

void SetSizes(const std::vector<std::pair<unsigned short, int>>& levels)
{
    s_sizes = levels;
}

void SetGrowth(const std::vector<std::pair<unsigned short, int>>& percents)
{
    s_growth = percents;
}

float SizeFactor(unsigned short key)
{
    for (const auto& entry : s_sizes)
    {
        if (entry.first == key)
            return 1.0f + SIZE_PER_LEVEL * static_cast<float>(entry.second - 1);
    }
    for (const auto& entry : s_growth)
    {
        if (entry.first == key)
            return static_cast<float>(entry.second) / 100.0f;
    }
    return 1.0f;
}

const State& Get()
{
    static const State s_none;
    return (WorldTime - s_updatedMs > STALE_AFTER_MS || !s_state.present) ? s_none : s_state;
}

bool Controlling()
{
    return Get().present && s_localControl;
}

void ToggleControl()
{
    if (!Get().present)
        return;

    // X commands it; X again releases it where it stands.
    s_localControl = !s_localControl;
    SendMobaSummonControl(s_localControl ? 1 : 0);
}

void CallBack()
{
    if (!Get().present)
        return;

    s_localControl = false;
    SendMobaSummonControl(2);
}

void OrderMove(int tileX, int tileY)
{
    if (!Controlling() || tileX < 0 || tileX > 255 || tileY < 0 || tileY > 255)
        return;

    SendMobaSummonOrder(1, static_cast<unsigned char>(tileX), static_cast<unsigned char>(tileY), 0);
}

void OrderAttack(unsigned short key)
{
    if (!Controlling())
        return;

    SendMobaSummonOrder(2, 0, 0, key);
}

}
