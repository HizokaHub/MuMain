#include "stdafx.h"
#include "World/Moba/MobaCommander.h"

#include "Network/Server/WSclient.h"  // SendMobaCommanderOrder

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

    // A lane counts as pressed when the enemy has at least this many creeps there and 1.5 times yours.
    constexpr int PRESSURE_MIN_CREEPS = 6;

    State s_state;
    double s_lastNewsMs = 0.0;
    int s_activeLane = 1;   // mid

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
    if (length >= lanesAt + LANE_COUNT * 2)
    {
        for (int lane = 0; lane < LANE_COUNT; ++lane)
        {
            state.ownCreeps[lane] = ReceiveBuffer[lanesAt + lane];
            state.enemyCreeps[lane] = ReceiveBuffer[lanesAt + LANE_COUNT + lane];
        }
    }

    s_state = state;
    s_lastNewsMs = NowMs();
}

const State& Get()
{
    if (s_state.visible && NowMs() > s_lastNewsMs + STALE_MS)
        s_state = State{};

    return s_state;
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
