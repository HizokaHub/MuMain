#pragma once

#include <string>
#include <vector>

// MOBA room list: the open private rooms the room browser shows. The server sends the list with
// C2 hi lo D5 1F  open  count  count * (id(u32) nameLen name hostLen host teamSize flags players age(u16) idle(u16))
// (open 1 = the browser opens, 0 = only refreshes if it is open), and the browser asks for a fresh one with C1 04 D5 1E, creates a
// room with C1 len D5 20 and joins one with C1 08 D5 21. The browser is drawn by SEASON3B::CMobaRoomBrowser; this module holds
// the state and talks to the server.
namespace World::Moba::RoomList
{
    constexpr unsigned char FLAG_MID_ONLY = 1;
    constexpr unsigned char FLAG_BOTS = 2;
    constexpr unsigned char FLAG_LOCKED = 4;

    struct Entry
    {
        unsigned int id = 0;
        std::wstring name;
        std::wstring host;
        int teamSize = 0;
        bool midOnly = false;
        bool bots = false;
        bool locked = false;   // asks for a password
        int players = 0;
        int ageSeconds = 0;
        int idleSeconds = 0;
    };

    struct State
    {
        bool open = false;
        std::vector<Entry> rooms;
    };

    // What the server sent; opens the browser when asked to.
    void Show(std::vector<Entry> rooms, bool open);

    // The current state.
    const State& Get();

    // Counts the lists received (the browser knows when to refresh).
    unsigned int Version();

    // Asks the server for a fresh list.
    void Refresh();

    // Creates a room (the server answers with the panel of the new room, which closes the browser).
    // The password may be empty (a room without one).
    void Create(const std::wstring& name, int teamSize, bool midOnly, bool bots, const std::wstring& password);

    // Joins a room (a failed attempt leaves the browser open; the server says why). The password may be empty.
    void Join(unsigned int id, const std::wstring& password);

    // Closes the browser.
    void Close();
}
