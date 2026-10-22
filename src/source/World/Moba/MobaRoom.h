#pragma once

#include <string>
#include <vector>

// MOBA private room: the state of the room the player is in. The server sends it every time something changes with
// C2 hi lo D5 1B  present  isHost  fillBots  midOnly  teamSize  id(u32)  nameLen name  blueCount (flags baseClass nameLen name)*  redCount (...)*
// (present 0 = the player is not in a room any more) and the chat lines with C2 hi lo D5 23 kind nameLen name textLen(u16) text.
// The buttons answer with C1 05 D5 1C action, the host's buttons on a player with C1 len D5 22 action nameLen name and the chat
// box with C1 len D5 24 textLen text. The panel is drawn by SEASON3B::CMobaRoomPanel; this module holds the state and talks to
// the server.
namespace World::Moba::Room
{
    // What a button asks the server to do (the values are part of the packet).
    enum class Action : unsigned char
    {
        SwitchTeam = 1,
        ToggleBots = 2,
        Start = 3,
        Leave = 4,
        CloseRoom = 5,
        Ready = 6,         // toggles "ready" (everybody but the host has to be ready for the host to start)
        Skills = 7,        // opens the loadout window from the room
        InviteParty = 8,   // invites the party members to the room
    };

    // What the host's buttons of a player ask (the values are part of the packet).
    enum class TargetAction : unsigned char
    {
        Move = 1,
        Kick = 2,
        MakeHost = 3,
    };

    struct Member
    {
        std::wstring name;
        bool host = false;
        bool mine = false;
        unsigned char baseClass = 0;   // 0 Dark Wizard .. 6 Rage Fighter: the portrait
        bool ready = false;
        bool hasSkills = false;        // picked the 4 skills of the match already
    };

    struct ChatLine
    {
        std::wstring name;   // empty for a notice of the room
        std::wstring text;
        bool notice = false;
    };

    struct State
    {
        bool present = false;
        bool isHost = false;
        bool fillBots = false;
        bool midOnly = false;
        bool locked = false;           // has a password
        int teamSize = 0;
        unsigned int id = 0;
        std::wstring name;
        std::vector<Member> blue;
        std::vector<Member> red;
    };

    // Shows or updates the panel with what the server sent. A different room (or none) clears the chat.
    void Show(State state);

    // The current state.
    const State& Get();

    // Counts the updates the server sent (the panel reloads its portraits when it changes).
    unsigned int Version();

    // A line of the chat of the room.
    void AddChatLine(ChatLine line);

    // The last lines of the chat, the oldest first; they change with ChatVersion.
    const std::vector<ChatLine>& Chat();
    unsigned int ChatVersion();

    // A short description of the match ("3v3 - carril central").
    std::wstring Label(const State& state);

    // Asks the server to do something in the room.
    void Request(Action action);

    // Asks the server for something the host does to another player of the room.
    void RequestOnPlayer(TargetAction action, const std::wstring& playerName);

    // Writes a line in the chat of the room.
    void SendChat(const std::wstring& text);

    // An invitation to a room (C2 D5 26: id, seconds, room name, who invites). It is shown in a popup until it is answered or expires.
    struct Invite
    {
        bool active = false;
        unsigned int id = 0;
        std::wstring roomName;
        std::wstring from;
    };

    void ShowInvite(unsigned int id, std::wstring roomName, std::wstring from, int seconds);

    // The pending invitation (inactive once it expired).
    const Invite& CurrentInvite();

    // Milliseconds left to answer the invitation.
    double InviteRemainingMs();

    // Accepts (joins the room) or declines (drops it) the pending invitation.
    void AnswerInvite(bool accept);
}
