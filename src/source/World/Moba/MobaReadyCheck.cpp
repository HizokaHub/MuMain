#include "stdafx.h"
#include "World/Moba/MobaReadyCheck.h"

#include "Network/Server/WSclient.h"  // SendMobaNpcMenuSelect

#include <algorithm>
#include <chrono>

namespace World::Moba::ReadyCheck
{

namespace
{
    constexpr int PACKET_BYTES = 15;
    constexpr int OFFSET_STATUS = 4;
    constexpr int OFFSET_REMAINING = 5;
    constexpr int OFFSET_WINDOW = 9;
    constexpr int OFFSET_ACCEPTED = 13;
    constexpr int OFFSET_TOTAL = 14;

    // The "match found" question is menu 3 of the server-driven menus (menu 2 is the queue master's); option 0 accepts.
    constexpr BYTE READY_MENU_ID = 3;
    constexpr BYTE OPTION_ACCEPT = 0;
    constexpr BYTE OPTION_DECLINE = 1;

    // The popup stays this long after the countdown if the server says nothing (a lost "hide" must not leave it up).
    constexpr double GRACE_MS = 2000.0;

    State s_state;
    double s_deadlineMs = 0.0;

    double NowMs()
    {
        using namespace std::chrono;
        return static_cast<double>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }

    unsigned int ReadU32(const unsigned char* bytes)
    {
        return static_cast<unsigned int>(bytes[0]) | (static_cast<unsigned int>(bytes[1]) << 8)
            | (static_cast<unsigned int>(bytes[2]) << 16) | (static_cast<unsigned int>(bytes[3]) << 24);
    }
}

void Receive(const unsigned char* ReceiveBuffer)
{
    if (ReceiveBuffer[1] < PACKET_BYTES)
        return;

    const unsigned char status = ReceiveBuffer[OFFSET_STATUS];
    if (status > static_cast<unsigned char>(Status::Accepted))
        return;

    s_state.status = static_cast<Status>(status);
    s_state.windowMs = static_cast<double>(ReadU32(ReceiveBuffer + OFFSET_WINDOW));
    s_state.accepted = ReceiveBuffer[OFFSET_ACCEPTED];
    s_state.total = ReceiveBuffer[OFFSET_TOTAL];
    s_deadlineMs = NowMs() + static_cast<double>(ReadU32(ReceiveBuffer + OFFSET_REMAINING));
}

const State& Get()
{
    if (s_state.status != Status::Hidden && NowMs() > s_deadlineMs + GRACE_MS)
        s_state = State{};

    return s_state;
}

double RemainingMs()
{
    return std::max(0.0, s_deadlineMs - NowMs());
}

void Answer(bool accept)
{
    if (s_state.status != Status::Asking)
        return;

    SendMobaNpcMenuSelect(READY_MENU_ID, accept ? OPTION_ACCEPT : OPTION_DECLINE);
    if (accept)
    {
        s_state.status = Status::Accepted;
        s_state.accepted = std::min(s_state.accepted + 1, s_state.total);
    }
    else
    {
        s_state = State{};
    }
}

}
