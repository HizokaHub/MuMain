#pragma once

#include <vector>

// MOBA loadout: the skills the player takes into a match. The server opens the window with
// C1 len D5 19  baseClass  min  max  selectableCount  selectedCount  selectable(u16 LE)...  selected(u16 LE)...
// and the player answers the save button with C1 len D5 1A  count  skill(u16 LE)... The window is drawn by
// SEASON3B::CMobaLoadoutWindow; this module holds the state and talks to the server.
namespace World::Moba::Loadout
{
    struct State
    {
        bool open = false;
        int baseClass = 0;
        int minSkills = 0;
        int maxSkills = 0;
        std::vector<unsigned short> selectable;
        std::vector<unsigned short> selected;
    };

    // Opens the window with what the server sent.
    void Open(State state);

    // The current state.
    const State& Get();

    // Whether the skill is in the current pick.
    bool IsSelected(unsigned short skill);

    // Picks or drops a skill; picking is ignored when the pick is full.
    void Toggle(unsigned short skill);

    // Whether the pick has a valid number of skills.
    bool IsComplete();

    // Sends the pick to the server and closes the window; ignored while the pick is not complete.
    void Save();

    // Closes the window without saving.
    void Close();
}
