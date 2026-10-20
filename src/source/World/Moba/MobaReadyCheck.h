#pragma once

// MOBA ready check: the state of the "match found" popup. The server sends it with
// C1 0F D5 15  status  remainingMs(u32 LE)  windowMs(u32 LE)  accepted  total
// (status 0 = hide, 1 = asking, 2 = accepted and waiting for the others) every time something changes, and the
// player answers with the NPC-menu packet C1 06 D5 07 menuId=3 option (0 = accept, 1 = decline).
// The popup itself is drawn by SEASON3B::CMobaReadyCheckWindow; this module holds the state and talks to the server.
namespace World::Moba::ReadyCheck
{
    enum class Status : unsigned char
    {
        Hidden = 0,
        Asking = 1,
        Accepted = 2,
    };

    struct State
    {
        Status status = Status::Hidden;
        int accepted = 0;
        int total = 0;
        double windowMs = 0.0;
    };

    // Handles the packet C1 0F D5 15.
    void Receive(const unsigned char* ReceiveBuffer);

    // The current state; it turns to Hidden by itself shortly after the countdown ran out without news from the server.
    const State& Get();

    // The milliseconds left to answer (0 when the countdown ran out).
    double RemainingMs();

    // Answers the question: sends the answer to the server and updates the popup at once.
    void Answer(bool accept);
}
