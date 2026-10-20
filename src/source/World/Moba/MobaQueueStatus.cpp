#include "stdafx.h"
#include "World/Moba/MobaQueueStatus.h"

#include "Network/Server/WSclient.h"  // SendMobaNpcMenuSelect

#include <algorithm>
#include <chrono>

namespace World::Moba::QueueStatus
{

namespace
{
    constexpr int FIXED_BYTES = 13;
    constexpr int STAGE_BYTES = 3;
    constexpr int OFFSET_MODE = 4;
    constexpr int OFFSET_ELAPSED = 5;
    constexpr int OFFSET_WAITING = 7;
    constexpr int OFFSET_NEXT_IN = 8;
    constexpr int OFFSET_NEXT_HUMANS = 10;
    constexpr int OFFSET_NEEDED = 11;
    constexpr int OFFSET_STAGE_COUNT = 12;
    constexpr unsigned int NO_NEXT = 0xFFFF;

    // The queue panel's cancel button is menu 4 of the server-driven menus; option 0 leaves the queue.
    constexpr BYTE PANEL_MENU_ID = 4;
    constexpr BYTE OPTION_LEAVE = 0;

    // The panel goes away this long after the server's last refresh (the server sends one a second).
    constexpr double STALE_MS = 3500.0;
    constexpr double MS_PER_SECOND = 1000.0;

    State s_state;
    double s_receivedMs = 0.0;
    double s_elapsedAtReceipt = 0.0;
    double s_nextInAtReceipt = -1.0;

    double NowMs()
    {
        using namespace std::chrono;
        return static_cast<double>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }

    unsigned int ReadU16(const unsigned char* bytes)
    {
        return static_cast<unsigned int>(bytes[0]) | (static_cast<unsigned int>(bytes[1]) << 8);
    }
}

void Receive(const unsigned char* ReceiveBuffer)
{
    const int length = ReceiveBuffer[1];
    if (length < FIXED_BYTES || ReceiveBuffer[OFFSET_MODE] > static_cast<unsigned char>(Mode::Aram))
        return;

    const int stageCount = ReceiveBuffer[OFFSET_STAGE_COUNT];
    if (length < FIXED_BYTES + stageCount * STAGE_BYTES)
        return;

    State state;
    state.mode = static_cast<Mode>(ReceiveBuffer[OFFSET_MODE]);
    state.waiting = ReceiveBuffer[OFFSET_WAITING];
    state.nextHumans = ReceiveBuffer[OFFSET_NEXT_HUMANS];
    state.humansNeeded = ReceiveBuffer[OFFSET_NEEDED];
    for (int i = 0; i < stageCount; ++i)
    {
        const unsigned char* stage = ReceiveBuffer + FIXED_BYTES + i * STAGE_BYTES;
        state.stages.push_back({ static_cast<int>(ReadU16(stage)), stage[2] });
    }

    const unsigned int nextIn = ReadU16(ReceiveBuffer + OFFSET_NEXT_IN);
    s_nextInAtReceipt = nextIn == NO_NEXT ? -1.0 : static_cast<double>(nextIn);
    s_elapsedAtReceipt = static_cast<double>(ReadU16(ReceiveBuffer + OFFSET_ELAPSED));
    s_receivedMs = NowMs();
    s_state = std::move(state);
}

const State& Get()
{
    if (s_state.mode != Mode::Hidden && NowMs() - s_receivedMs > STALE_MS)
        s_state = State{};

    return s_state;
}

double ElapsedSeconds()
{
    return s_elapsedAtReceipt + (NowMs() - s_receivedMs) / MS_PER_SECOND;
}

double NextInSeconds()
{
    if (s_nextInAtReceipt < 0.0)
        return -1.0;

    return std::max(0.0, s_nextInAtReceipt - (NowMs() - s_receivedMs) / MS_PER_SECOND);
}

void Cancel()
{
    SendMobaNpcMenuSelect(PANEL_MENU_ID, OPTION_LEAVE);
    s_state = State{};
}

}
