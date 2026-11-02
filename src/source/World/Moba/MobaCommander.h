#pragma once

// MOBA commanders mode: the state of the commander panel. The server sends it about once a second to every champion of a
// commanders match with
// C1 len D5 40  flags  essence(u16 LE)  income  level  maxLevel  nextCost(u16 LE)  laneMask  unitCount  cost(u16 LE) x unitCount
//   ownCreeps x 3  enemyCreeps x 3 (the creeps alive in each lane, top mid bot; optional: older servers stop after the costs)
//   waveSeconds  waveInterval  sendNowCooldown  queued[lane][unit] x (3 x unitCount) (direct-strike extension, see below)
// (flags: bit 0 visible, bit 1 you command the team, bit 2 the command is free), and the panel answers with
// C1 07 D5 41  action  unit  lane. The panel itself is drawn by SEASON3B::CMobaCommanderWindow; this module holds the state
// and talks to the server.
//
// Direct-strike extension (2026-11-01): the packs bought are held in a waiting zone of the base until a timer, common to both teams,
// releases them all at once (or the commander uses "send now"). After the lane counts the server appends
//   waveSeconds (u8)     seconds to the next wave (0 = no timer)
//   waveInterval (u8)    total seconds of a wave
//   sendNowCooldown (u8) seconds until "send now" can be used again (0 = ready)
//   queued[lane][unit] (u8)  PACKS held right now in the waiting zone of YOUR team, lane-major (top, mid, bot) x unitCount
// An older server stops earlier: the client keeps those fields at 0.
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
        SendNow = 4,   // release the packs held in the waiting zone of your team now (unit and lane are ignored)
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
        int waveSeconds = 0;                // seconds to the next wave when the packet was sent (0 = no timer)
        int waveInterval = 0;               // seconds between two waves (0 = no timer)
        int sendNowCooldown = 0;            // seconds until "send now" is ready when the packet was sent
        int queued[LANE_COUNT][MAX_UNITS] = {};   // packs held in the waiting zone of your team, per lane and unit
    };

    // The lane (0 top, 1 mid, 2 bot) the pack keys send to; the commander picks it with 1/2/3 or by clicking a lane card.
    int ActiveLane();
    void SetActiveLane(int lane);

    // Whether the enemy is pressing a lane: clearly more of its creeps than yours.
    bool LaneUnderPressure(const State& state, int lane);

    // Whether the player commands a running match.
    bool CommandingNow();

    // Whether the client shows the RTS view: the player commands a running commanders match. Then the camera is forced to the
    // overhead MOBA camera, the hero is not drawn nor controlled, and the hero HUD gives way to the RTS console. It has hysteresis:
    // a late packet does not turn it off (it holds on for RTS_HOLD_MS without news), an explicit "you do not command" does.
    bool RtsView();

    // Where the RTS console starts (reference px, 640x480): the camera keeps the edge scroll out of it.
    constexpr int RTS_CONSOLE_TOP = 380;

    // The width of the box of the console that the minimap takes (left end; the minimap draws itself there in the RTS view).
    constexpr int RTS_MINIMAP_WIDTH = 106;

    // The centre of a lane in tile units (0 top, 1 mid, 2 bot): the halfway point along the lane path of the arena layout
    // (MobaLayout.json of the server: top x 24..25, mid x 116, bot x 217 around y 125..128). F1/F2/F3 point the camera there.
    void LaneFocusTile(int lane, float& tileX, float& tileY);

    // Seconds to the next wave / until "send now" is ready, counted down locally between the one-second packets (0 = none / ready).
    int WaveSecondsLeft();
    int SendNowSecondsLeft();

    // Handles the packet C1 len D5 40.
    void Receive(const unsigned char* ReceiveBuffer);

    // The current state; it turns hidden by itself when the server stops sending news (the match ended).
    const State& Get();

    // Whether a lane (0 top, 1 mid, 2 bot) is open in this match.
    bool IsLaneOpen(const State& state, int lane);

    // Asks the server for something (a button of the panel).
    void Order(Action action, int unit = 0, int lane = 0);
}
