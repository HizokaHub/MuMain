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
    // An empty list of skills is the server closing the window (the match starts).
    if (state.selectable.empty())
    {
        Close();
        return;
    }

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
        SendMobaLoadout(s_state.selected);
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
