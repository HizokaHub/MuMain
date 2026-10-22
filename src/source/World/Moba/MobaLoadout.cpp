#include "stdafx.h"
#include "World/Moba/MobaLoadout.h"

#include "Network/Server/WSclient.h"  // SendMobaLoadout

#include <algorithm>

namespace World::Moba::Loadout
{

namespace
{
    State s_state;

    int PickedCount()
    {
        return static_cast<int>(s_state.selected.size());
    }

    // The summons of the Elf are the skills 30-36 (Summon Goblin .. Summon Soldier).
    bool IsSummonSkill(unsigned short skill)
    {
        return skill >= 30 && skill <= 36;
    }

    // What the server gets: the skills picked and, when the class has one, the appearance of the summon.
    std::vector<unsigned short> Composed()
    {
        std::vector<unsigned short> all = s_state.selected;
        if (s_state.summon != 0)
            all.push_back(s_state.summon);
        return all;
    }
}

void Open(State state)
{
    // An empty list of skills is the server closing the window (the match starts).
    if (state.selectable.empty())
    {
        Close();
        return;
    }

    // The creatures are not part of the 4 picks: they are the appearance of the 5th skill.
    std::vector<unsigned short> plain;
    for (const unsigned short skill : state.selectable)
    {
        if (IsSummonSkill(skill))
            state.summons.push_back(skill);
        else
            plain.push_back(skill);
    }

    std::vector<unsigned short> picks;
    for (const unsigned short skill : state.selected)
    {
        if (IsSummonSkill(skill))
            state.summon = skill;
        else
            picks.push_back(skill);
    }

    if (state.summon == 0 && !state.summons.empty())
        state.summon = state.summons.front();

    state.selectable = std::move(plain);
    state.selected = std::move(picks);
    s_state = std::move(state);
    s_state.open = true;
}

const State& Get()
{
    return s_state;
}

bool IsSelected(unsigned short skill)
{
    return std::find(s_state.selected.begin(), s_state.selected.end(), skill) != s_state.selected.end();
}

int PlaceOf(unsigned short skill)
{
    const auto picked = std::find(s_state.selected.begin(), s_state.selected.end(), skill);
    return picked == s_state.selected.end() ? 0 : static_cast<int>(picked - s_state.selected.begin()) + 1;
}

void PickSummon(unsigned short skill)
{
    if (!IsSummonSkill(skill))
        return;

    s_state.summon = skill;
    if (IsComplete())
        SendMobaLoadout(Composed());
}

void Toggle(unsigned short skill)
{
    const auto picked = std::find(s_state.selected.begin(), s_state.selected.end(), skill);
    if (picked != s_state.selected.end())
    {
        s_state.selected.erase(picked);
        return;
    }

    if (PickedCount() >= s_state.maxSkills)
        s_state.selected.erase(s_state.selected.begin());

    s_state.selected.push_back(skill);
    if (IsComplete())
        SendMobaLoadout(Composed());
}

bool IsComplete()
{
    return PickedCount() >= s_state.minSkills && PickedCount() <= s_state.maxSkills;
}

void Save()
{
    if (!s_state.open || !IsComplete())
        return;

    SendMobaLoadout(Composed());
    Close();
}

void Close()
{
    s_state = State{};
}

}
