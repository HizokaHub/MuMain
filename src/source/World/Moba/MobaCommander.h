#pragma once

// MOBA commanders mode: the state of the commander panel. The server sends it about once a second to every champion of a
// commanders match with
// C1 len D5 40  flags  essence(u16 LE)  income  level  maxLevel  nextCost(u16 LE)  laneMask  unitCount  cost(u16 LE) x unitCount
//   ownCreeps x 3  enemyCreeps x 3 (the creeps alive in each lane, top mid bot; optional: older servers stop after the costs)
// (flags: bit 0 visible, bit 1 you command the team, bit 2 the command is free), and the panel answers with
// C1 07 D5 41  action  unit  lane. The panel itself is drawn by SEASON3B::CMobaCommanderWindow; this module holds the state
// and talks to the server.
namespace World::Moba::Commander
{
    constexpr int MAX_UNITS = 6;
    constexpr int LANE_COUNT = 3;

    // What a button asks the server for (the action byte of C1 07 D5 41).
    enum class Action : unsigned char
    {
        Send = 0,
        Income = 1,
        Claim = 2,
        Release = 3,
    };

    struct State
    {
        bool visible = false;
        bool isCommander = false;
        bool commandFree = false;
        int essence = 0;
        int incomePerSecond = 0;
        int incomeLevel = 0;
        int maxIncomeLevel = 0;
        int nextIncomeCost = 0;   // 0 when the income is at its highest level
        int openLaneMask = 0;     // bit 0 top, bit 1 mid, bit 2 bot
        int unitCount = 0;
        int unitCost[MAX_UNITS] = {};
        int ownCreeps[LANE_COUNT] = {};     // live creeps of your team in each lane
        int enemyCreeps[LANE_COUNT] = {};   // live creeps of the enemy team in each lane
    };

    // The lane (0 top, 1 mid, 2 bot) the pack keys send to; the commander picks it with 1/2/3 or by clicking a lane card.
    int ActiveLane();
    void SetActiveLane(int lane);

    // Whether the enemy is pressing a lane: clearly more of its creeps than yours.
    bool LaneUnderPressure(const State& state, int lane);

    // Whether the player commands a running match: the camera then opens the wide RTS overview and the pack keys are taken.
    bool CommandingNow();

    // Handles the packet C1 len D5 40.
    void Receive(const unsigned char* ReceiveBuffer);

    // The current state; it turns hidden by itself when the server stops sending news (the match ended).
    const State& Get();

    // Whether a lane (0 top, 1 mid, 2 bot) is open in this match.
    bool IsLaneOpen(const State& state, int lane);

    // Asks the server for something (a button of the panel).
    void Order(Action action, int unit = 0, int lane = 0);
}
