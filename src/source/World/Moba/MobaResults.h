#pragma once

#include <string>
#include <vector>

// MOBA match results: the table the server sends when a nexus falls. The packet is
// C2 hi lo D5 18  winner  duration(u32 LE)  secondsToReturn(u16 LE)  count  count * (row, see WSclient.cpp)
// and the player answers the "back to town" button with C1 04 D5 18, after which the server closes the session and the client
// reconnects by itself into the real character. The table is drawn by SEASON3B::CMobaResultsWindow; this module holds the
// state and talks to the server.
namespace World::Moba::Results
{
    constexpr unsigned char TEAM_BLUE = 1;
    constexpr unsigned char TEAM_RED = 2;

    // One champion's line of the table.
    struct Row
    {
        std::wstring name;
        unsigned char team = 0;
        unsigned char baseClass = 0;
        bool mine = false;
        bool bot = false;
        int level = 0;
        int kills = 0;
        int deaths = 0;
        int assists = 0;
        int creeps = 0;
        unsigned int gold = 0;
        unsigned int damageDealt = 0;
        unsigned int damageTaken = 0;
    };

    struct State
    {
        bool visible = false;
        unsigned char winner = 0;
        unsigned int durationSeconds = 0;
        std::vector<Row> rows;
    };

    // Shows the table; the server sends the player back on its own after secondsToReturn.
    void Show(State state, int secondsToReturn);

    // The current state.
    const State& Get();

    // The milliseconds left until the server sends the player back (0 once they ran out).
    double RemainingMs();

    // Whether the player's own team won (false for a spectator without a row of their own).
    bool IsVictory();

    // Hides the table.
    void Hide();

    // Asks the server to send the player back to town and hides the table.
    void ReturnToTown();
}
