#pragma once

#include <string>
#include <vector>

// MOBA: the control of a disconnected teammate. The server lists the champions of the team without player (C2 D5 30: key, class, life, mana, name) and, for the one
// the player controls, its skills with their cooldowns. A short tap of Ctrl takes the nearest one (or lets go of the controlled one); while controlling, the right
// click orders it (walk / attack), the keys 1-9 cast ITS skills (the owner's own key setup is never touched: the mapping below lives only in this client), B sends it
// to its base. The commands go out as C1 07 D5 31 (take / release), C1 09 D5 32 (order), C1 0A D5 33 (cast) and C1 04 D5 34 (recall).
namespace World::Moba::Puppet
{
    constexpr int SLOT_COUNT = 9;   // the keys 1-9

    struct Candidate
    {
        unsigned short key = 0;
        int baseClass = 0;        // portrait
        int hpPercent = 0;
        int mpPercent = 0;
        bool selected = false;
        std::wstring name;
    };

    struct Skill
    {
        int number = 0;
        int rank = 0;             // 0 = inactive until the owner puts a point in it
        bool isBuff = false;
        double cooldownEndMs = 0.0;   // WorldTime when it is ready again
        double cooldownFullMs = 0.0;
    };

    struct State
    {
        bool controlling = false;
        int x = 0;                // tile of the controlled champion
        int y = 0;
        std::vector<Candidate> candidates;
        std::vector<Skill> skills;
    };

    // Replaces the state with what the server sent.
    void Set(State state);

    // The current state (empty once the server stopped sending it).
    const State& Get();

    // Whether the player controls a champion right now.
    bool Controlling();

    // The controlled champion's data, or nullptr.
    const Candidate* Controlled();

    // A short tap of Ctrl: takes the nearest teammate without player, or lets go of the controlled one.
    void ToggleControl();

    // Takes the champion with that key (or lets it go when it is the controlled one): the click on its card.
    void Select(unsigned short key);

    // Orders for the controlled champion: walk to a tile, attack the object with that key.
    void OrderMove(int tileX, int tileY);
    void OrderAttack(unsigned short key);

    // Casts a skill of the controlled champion on an object (key 0xFFFF = none) or a tile.
    void Cast(int skillNumber, unsigned short key, int tileX, int tileY);

    // Key B: the controlled champion goes to its base.
    void Recall();

    // The skill bound to a key slot (0 = key 1 ... 8 = key 9), or 0.
    int SkillOfSlot(int slot);

    // Binds a skill to a key slot (the click on a skill and then a number key); the skill that had the slot takes the old one.
    void Rebind(int skillNumber, int slot);

    // The skill waiting for a key (clicked on the bar), or 0.
    int RebindPending();
    void SetRebindPending(int skillNumber);

    // The skill of the controlled champion with that number, or nullptr.
    const Skill* FindSkill(int skillNumber);

    // Called once per frame from the key handler: the Ctrl tap, and (while controlling) the number keys. Returns true when it used a key.
    bool UpdateKeys();
}
