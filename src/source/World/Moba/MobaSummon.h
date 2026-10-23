#pragma once

#include <utility>
#include <vector>

// MOBA summon: the creature of the Elf (her 5th skill). The server sends its state about once a second with
// C1 0C D5 29  present  mode (0 follows, 1 commanded, 2 holds its post)  keyLo keyHi  hp%  tileX  tileY  rank
// (present 0 = she has none), and the player commands it with C1 05 D5 27 mode (1 = command it with X, 0 = release it where it stands with X again, 2 = call it back with Z) and C1 09 D5 28 kind x y keyLo keyHi
// (kind 1 = walk to the tile, 2 = attack the object with that key). While the control mode is on the right click gives the
// orders (the ground, the minimap or an enemy) instead of the hero's skill. It is drawn by SEASON3B::CMobaSummonHud and the
// minimap; this module holds the state and talks to the server.
namespace World::Moba::Summon
{
    struct State
    {
        bool present = false;
        bool control = false;     // the player commands it (mode 1)
        bool stay = false;        // released where it stands, holding its post (mode 2 of the packet)
        unsigned short key = 0;   // object id of the creature
        int hpPercent = 0;
        int x = 0;                // tile
        int y = 0;
        int rank = 0;             // rank of the skill, 1-5
        int level = 1;            // level of the creature itself, 1-10 (it levels up with what it kills)
        int progress = 0;         // percent towards the next level (100 at the highest)
    };

    // Replaces the state with what the server sent.
    void Set(State state);

    // The current state (nothing once the server stopped sending it).
    const State& Get();

    // Whether the player is commanding the creature (it is alive and the mode is on).
    bool Controlling();

    // Key X: command the creature; pressed again it is released where it stands (it holds its post, it does not follow).
    void ToggleControl();

    // Key Z: call the creature back, it follows the player again.
    void CallBack();

    // The server sends the level of every living creature of the match (C1 D5 2B count [keyLo keyHi level]); each one is drawn bigger the higher
    // its level is. Replaces the whole list.
    void SetSizes(const std::vector<std::pair<unsigned short, int>>& levels);

    // The factor on the size of the creature with that object id (1 when it is not on the list).
    float SizeFactor(unsigned short key);

    // Orders: walk to a tile, or attack the enemy with that object id.
    void OrderMove(int tileX, int tileY);
    void OrderAttack(unsigned short key);
}
