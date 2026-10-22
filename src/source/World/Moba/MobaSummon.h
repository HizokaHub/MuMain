#pragma once

// MOBA summon: the creature of the Elf (her 5th skill). The server sends its state about once a second with
// C1 0C D5 29  present  control  keyLo keyHi  hp%  tileX  tileY  rank
// (present 0 = she has none), and the player commands it with C1 05 D5 27 on (control key X) and C1 09 D5 28 kind x y keyLo keyHi
// (kind 1 = walk to the tile, 2 = attack the object with that key). While the control mode is on the right click gives the
// orders (the ground, the minimap or an enemy) instead of the hero's skill. It is drawn by SEASON3B::CMobaSummonHud and the
// minimap; this module holds the state and talks to the server.
namespace World::Moba::Summon
{
    struct State
    {
        bool present = false;
        bool control = false;
        unsigned short key = 0;   // object id of the creature
        int hpPercent = 0;
        int x = 0;                // tile
        int y = 0;
        int rank = 0;             // rank of the skill, 1-5
    };

    // Replaces the state with what the server sent.
    void Set(State state);

    // The current state (nothing once the server stopped sending it).
    const State& Get();

    // Whether the player is commanding the creature (it is alive and the mode is on).
    bool Controlling();

    // Key X: command the creature, or give it back to follow.
    void ToggleControl();

    // Orders: walk to a tile, or attack the enemy with that object id.
    void OrderMove(int tileX, int tileY);
    void OrderAttack(unsigned short key);
}
