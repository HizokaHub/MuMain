#include "stdafx.h"
#include "World/Moba/MobaCommander.h"

#include "Network/Server/WSclient.h"  // SendMobaCommanderOrder
#include "World/MapInfra/MapManager.h"  // gMapManager, WD_200_MOBA_ARENA

#include <algorithm>
#include <chrono>

namespace World::Moba::Commander
{

namespace
{
    constexpr int HEADER_BYTES = 14;
    constexpr int COST_BYTES = 2;
    constexpr int OFFSET_FLAGS = 4;
    constexpr int OFFSET_ESSENCE = 5;
    constexpr int OFFSET_INCOME = 7;
    constexpr int OFFSET_LEVEL = 8;
    constexpr int OFFSET_MAX_LEVEL = 9;
    constexpr int OFFSET_NEXT_COST = 10;
    constexpr int OFFSET_LANE_MASK = 12;
    constexpr int OFFSET_UNIT_COUNT = 13;

    constexpr unsigned char FLAG_VISIBLE = 1;
    constexpr unsigned char FLAG_COMMANDER = 2;
    constexpr unsigned char FLAG_FREE = 4;

    // The panel goes away when the server has been silent this long (it sends news every second while the match runs).
    constexpr double STALE_MS = 3000.0;

    // The RTS view of the commander is more patient: a late packet must not flick the camera, the hero and the HUD back and forth.
    constexpr double RTS_HOLD_MS = 8000.0;

    // A lane counts as pressed when the enemy has at least this many creeps there and 1.5 times yours.
    constexpr int PRESSURE_MIN_CREEPS = 6;

    // Direct-strike extension of the packet (after the lane counts): three timer bytes and the held packs per lane and unit.
    constexpr int TIMER_BYTES = 3;

    // The centre of each lane in tile units: the arc-length midpoint of the lane polylines of MobaLayout.json (top, mid, bot).
    constexpr float LANE_FOCUS_X[LANE_COUNT] = { 24.4f, 116.0f, 216.7f };
    constexpr float LANE_FOCUS_Y[LANE_COUNT] = { 125.4f, 127.4f, 127.7f };

    State s_state;
    double s_lastNewsMs = 0.0;
    int s_activeLane = 1;   // mid

    // Whether the last packet said "you command" (the RTS view then holds on for RTS_HOLD_MS), and when it was received.
    bool s_commanderNews = false;

    // The timers counted down locally: their value in the last packet and when it arrived.
    double s_timersAtMs = 0.0;

    double NowMs()
    {
        using namespace std::chrono;
        return static_cast<double>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }

    int ReadU16(const unsigned char* bytes)
    {
        return static_cast<int>(bytes[0]) | (static_cast<int>(bytes[1]) << 8);
    }
}

void Receive(const unsigned char* ReceiveBuffer)
{
    const int length = ReceiveBuffer[1];
    if (length < HEADER_BYTES)
        return;

    const int unitCount = ReceiveBuffer[OFFSET_UNIT_COUNT];
    if (unitCount > MAX_UNITS || length < HEADER_BYTES + unitCount * COST_BYTES)
        return;

    State state;
    const unsigned char flags = ReceiveBuffer[OFFSET_FLAGS];
    state.visible = (flags & FLAG_VISIBLE) != 0;
    state.isCommander = (flags & FLAG_COMMANDER) != 0;
    state.commandFree = (flags & FLAG_FREE) != 0;
    state.essence = ReadU16(ReceiveBuffer + OFFSET_ESSENCE);
    state.incomePerSecond = ReceiveBuffer[OFFSET_INCOME];
    state.incomeLevel = ReceiveBuffer[OFFSET_LEVEL];
    state.maxIncomeLevel = ReceiveBuffer[OFFSET_MAX_LEVEL];
    state.nextIncomeCost = ReadU16(ReceiveBuffer + OFFSET_NEXT_COST);
    state.openLaneMask = ReceiveBuffer[OFFSET_LANE_MASK];
    state.unitCount = unitCount;
    for (int i = 0; i < unitCount; ++i)
        state.unitCost[i] = ReadU16(ReceiveBuffer + HEADER_BYTES + i * COST_BYTES);

    const int lanesAt = HEADER_BYTES + unitCount * COST_BYTES;
    const int timersAt = lanesAt + LANE_COUNT * 2;
    if (length >= timersAt)
    {
        for (int lane = 0; lane < LANE_COUNT; ++lane)
        {
            state.ownCreeps[lane] = ReceiveBuffer[lanesAt + lane];
            state.enemyCreeps[lane] = ReceiveBuffer[lanesAt + LANE_COUNT + lane];
        }
    }

    // Older servers stop here (or after the timers): the fields they lack stay at 0.
    if (length >= timersAt + TIMER_BYTES)
    {
        state.waveSeconds = ReceiveBuffer[timersAt];
        state.waveInterval = ReceiveBuffer[timersAt + 1];
        state.sendNowCooldown = ReceiveBuffer[timersAt + 2];
    }

    const int queuedAt = timersAt + TIMER_BYTES;
    if (length >= queuedAt + LANE_COUNT * unitCount)
    {
        for (int lane = 0; lane < LANE_COUNT; ++lane)
        {
            for (int unit = 0; unit < unitCount; ++unit)
                state.queued[lane][unit] = ReceiveBuffer[queuedAt + lane * unitCount + unit];
        }
    }

    s_state = state;
    s_lastNewsMs = NowMs();
    s_timersAtMs = s_lastNewsMs;
    s_commanderNews = state.visible && state.isCommander;
}

const State& Get()
{
    // A commander keeps the console through a longer silence (see RtsView).
    const double staleMs = s_state.isCommander ? RTS_HOLD_MS : STALE_MS;
    if (s_state.visible && NowMs() > s_lastNewsMs + staleMs)
    {
        s_state = State{};
        s_commanderNews = false;
    }

    return s_state;
}

bool RtsView()
{
    const State& state = Get();
    return s_commanderNews && state.visible && state.isCommander
        && gMapManager.WorldActive == WD_200_MOBA_ARENA;
}

void LaneFocusTile(int lane, float& tileX, float& tileY)
{
    const int index = std::clamp(lane, 0, LANE_COUNT - 1);
    tileX = LANE_FOCUS_X[index];
    tileY = LANE_FOCUS_Y[index];
}

int WaveSecondsLeft()
{
    const State& state = Get();
    if (!state.visible || state.waveInterval <= 0)
        return 0;

    const int elapsed = static_cast<int>((NowMs() - s_timersAtMs) / 1000.0);
    return std::max(0, state.waveSeconds - elapsed);
}

int SendNowSecondsLeft()
{
    const State& state = Get();
    if (!state.visible)
        return 0;

    const int elapsed = static_cast<int>((NowMs() - s_timersAtMs) / 1000.0);
    return std::max(0, state.sendNowCooldown - elapsed);
}

int ActiveLane()
{
    return s_activeLane;
}

void SetActiveLane(int lane)
{
    s_activeLane = std::clamp(lane, 0, LANE_COUNT - 1);
}

bool LaneUnderPressure(const State& state, int lane)
{
    if (lane < 0 || lane >= LANE_COUNT)
        return false;

    const int enemy = state.enemyCreeps[lane];
    return enemy >= PRESSURE_MIN_CREEPS && enemy * 2 >= state.ownCreeps[lane] * 3;
}

bool CommandingNow()
{
    const State& state = Get();
    return state.visible && state.isCommander;
}

bool IsLaneOpen(const State& state, int lane)
{
    return lane >= 0 && lane < LANE_COUNT && (state.openLaneMask & (1 << lane)) != 0;
}

void Order(Action action, int unit, int lane)
{
    SendMobaCommanderOrder(static_cast<BYTE>(action), static_cast<BYTE>(unit), static_cast<BYTE>(lane));
}

}
