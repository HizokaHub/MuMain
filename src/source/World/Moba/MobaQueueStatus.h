#pragma once

#include <vector>

// MOBA queue status: how the player's search is going. The server sends it about once a second while the player waits:
// C1 len D5 16  mode  elapsed(u16 LE)  waiting  nextIn(u16 LE, 0xFFFF = none)  nextHumans  needed  stageCount  stageCount * (seconds(u16 LE) humans)
// (mode 0 = no panel). The panel is drawn by SEASON3B::CMobaQueuePanel; this module holds the state and talks to the server.
namespace World::Moba::QueueStatus
{
    enum class Mode : unsigned char
    {
        Hidden = 0,
        Open = 1,
        Premade = 2,
        WithBots = 3,
    };

    // One step of the wait for a match with bots: after this many seconds, this many humans are enough.
    struct Stage
    {
        int seconds = 0;
        int humans = 0;
    };

    struct State
    {
        Mode mode = Mode::Hidden;
        int waiting = 0;        // players waiting in the same pool
        int humansNeeded = 0;   // the humans a match with bots needs now (0 = too early)
        int nextHumans = 0;     // the humans needed at the next step
        std::vector<Stage> stages;
    };

    // Handles the packet C1 len D5 16.
    void Receive(const unsigned char* ReceiveBuffer);

    // The current state; it turns to Hidden by itself when the server stops refreshing it.
    const State& Get();

    // Seconds the player has waited (the server's count plus the time since it arrived).
    double ElapsedSeconds();

    // Seconds until the next step; negative when there is none.
    double NextInSeconds();

    // Leaves the queue: tells the server and hides the panel at once.
    void Cancel();
}
