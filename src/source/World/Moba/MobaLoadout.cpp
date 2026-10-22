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
}

void Open(State state)
{
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

void Toggle(unsigned short skill)
{
    const auto picked = std::find(s_state.selected.begin(), s_state.selected.end(), skill);
    if (picked != s_state.selected.end())
    {
        s_state.selected.erase(picked);
        return;
    }

    if (PickedCount() < s_state.maxSkills)
        s_state.selected.push_back(skill);
}

bool IsComplete()
{
    return PickedCount() >= s_state.minSkills && PickedCount() <= s_state.maxSkills;
}

void Save()
{
    if (!s_state.open || !IsComplete())
        return;

    SendMobaLoadout(s_state.selected);
    Close();
}

void Close()
{
    s_state = State{};
}

}
