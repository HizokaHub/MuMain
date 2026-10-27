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
        int appearance = 0;       // skill number 30-36 of the creature (0 = unknown)
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

    // The golden invasion sends the size of its growing dragons (C1 D5 2E count [keyLo keyHi percent]): a newborn is 35 % of a grown one. Replaces the list.
    void SetGrowth(const std::vector<std::pair<unsigned short, int>>& percents);

    // The factor on the size of the monster with that object id: the level of an Elf's creature or the growth of a golden dragon (1 when it is on neither list).
    float SizeFactor(unsigned short key);

    // The skills of the creature: one or two, on the keys 1 and 2 while the player commands it (C1 08 D5 38 slot keyLo keyHi casts one on the object under the
    // cursor; the server sends the cooldowns in tenths of a second at the end of the state packet).
    struct SkillInfo
    {
        const wchar_t* name;
        int iconSkill;            // the skill whose icon is drawn
        int cooldownSeconds;
        int minRank;              // rank of the summon skill from which the creature has it
    };

    // The number of skills of the creature (0 when unknown).
    int SkillCount();

    // The skill of a slot (0 or 1); only valid for slot < SkillCount().
    const SkillInfo& Skill(int slot);

    // Milliseconds left until the skill is ready again (0 when ready).
    double SkillRemainingMs(int slot);

    // Called with the cooldowns of the server (tenths of a second).
    void SetCooldowns(int firstTenths, int secondTenths);

    // The keys 1 and 2 while commanding the creature: asks for the skill to be cast on the object under the cursor (done in the world phase, see
    // MobaSummonCastAtCursor). Returns true when the key was taken.
    bool UpdateKeys();

    // Sends the cast of a skill on the object with that key (0xFFFF = none) and starts its cooldown at once.
    void Cast(int slot, unsigned short key);

    // Orders: walk to a tile, or attack the enemy with that object id.
    void OrderMove(int tileX, int tileY);
    void OrderAttack(unsigned short key);
}
