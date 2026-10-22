#pragma once

#include <string>
#include <vector>

// MOBA private room: the state of the room the player is in. The server sends it every time something changes with
// C2 hi lo D5 1B  present  isHost  fillBots  teamSize  codeLen code  labelLen label  blueCount (flags nameLen name)*  redCount (...)*
// (present 0 = the player is not in a room any more) and the player's buttons answer with C1 05 D5 1C action. The panel is
// drawn by SEASON3B::CMobaRoomPanel; this module holds the state and talks to the server.
namespace World::Moba::Room
{
    // What a button asks the server to do (the values are part of the packet).
    enum class Action : unsigned char
    {
        SwitchTeam = 1,
        ToggleBots = 2,
        Start = 3,
        Leave = 4,
    };

    struct Member
    {
        std::wstring name;
        bool host = false;
        bool mine = false;
        unsigned char baseClass = 0;   // 0 Dark Wizard .. 6 Rage Fighter: the portrait
    };

    struct State
    {
        bool present = false;
        bool isHost = false;
        bool fillBots = false;
        int teamSize = 0;
        std::wstring code;
        std::wstring label;
        std::vector<Member> blue;
        std::vector<Member> red;
    };

    // Shows or updates the panel with what the server sent.
    void Show(State state);

    // The current state.
    const State& Get();

    // Counts the updates the server sent (the panel reloads its portraits when it changes).
    unsigned int Version();

    // Asks the server to do something in the room.
    void Request(Action action);
}
