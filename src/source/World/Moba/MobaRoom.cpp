#include "stdafx.h"
#include "World/Moba/MobaRoom.h"

#include "Network/Server/WSclient.h"  // SendMobaRoomAction, SendMobaRoomTargetAction, SendMobaRoomChat
#include "World/Moba/MobaRoomList.h"
#include "Audio/DSPlaySound.h"        // PlayBuffer

#include <algorithm>
#include <chrono>

namespace World::Moba::Room
{

namespace
{
    constexpr size_t MAX_CHAT_LINES = 60;

    State s_state;
    unsigned int s_version = 1;
    std::vector<ChatLine> s_chat;
    unsigned int s_chatVersion = 1;
    Invite s_invite;
    double s_inviteExpiresMs = 0.0;

    double NowMs()
    {
        using namespace std::chrono;
        return static_cast<double>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }

    size_t MemberCount(const State& state)
    {
        return state.blue.size() + state.red.size();
    }
}

void Show(State state)
{
    if (!state.present || state.id != s_state.id)
    {
        s_chat.clear();
        ++s_chatVersion;
    }

    // The browser is not needed once the player is in a room.
    if (state.present)
        RoomList::Close();

    // Somebody joined the room the player is already in.
    if (state.present && s_state.present && state.id == s_state.id && MemberCount(state) > MemberCount(s_state))
        PlayBuffer(SOUND_CLICK01);

    s_state = std::move(state);
    ++s_version;
}

const State& Get()
{
    return s_state;
}

unsigned int Version()
{
    return s_version;
}

void AddChatLine(ChatLine line)
{
    bool mine = false;
    for (const std::vector<Member>* team : { &s_state.blue, &s_state.red })
    {
        for (const Member& member : *team)
            mine = mine || (member.mine && member.name == line.name);
    }

    if (!mine)
        PlayBuffer(SOUND_CLICK01);

    s_chat.push_back(std::move(line));
    if (s_chat.size() > MAX_CHAT_LINES)
        s_chat.erase(s_chat.begin());

    ++s_chatVersion;
}

const std::vector<ChatLine>& Chat()
{
    return s_chat;
}

unsigned int ChatVersion()
{
    return s_chatVersion;
}

std::wstring Label(const State& state)
{
    return std::to_wstring(state.teamSize) + L"v" + std::to_wstring(state.teamSize) + (state.midOnly ? L" - carril central" : L" - 3 carriles");
}

void Request(Action action)
{
    if (!s_state.present)
        return;

    SendMobaRoomAction(static_cast<unsigned char>(action));
}

void RequestOnPlayer(TargetAction action, const std::wstring& playerName)
{
    if (!s_state.present || !s_state.isHost)
        return;

    SendMobaRoomTargetAction(static_cast<unsigned char>(action), playerName);
}

void SendChat(const std::wstring& text)
{
    if (!s_state.present || text.empty())
        return;

    SendMobaRoomChat(text);
}

void ShowInvite(unsigned int id, std::wstring roomName, std::wstring from, int seconds)
{
    // Not while the player already is in a room.
    if (s_state.present)
        return;

    s_invite = { true, id, std::move(roomName), std::move(from) };
    s_inviteExpiresMs = NowMs() + seconds * 1000.0;
    PlayBuffer(SOUND_CLICK01);
}

const Invite& CurrentInvite()
{
    if (s_invite.active && (NowMs() > s_inviteExpiresMs || s_state.present))
        s_invite = Invite{};

    return s_invite;
}

double InviteRemainingMs()
{
    return std::max(0.0, s_inviteExpiresMs - NowMs());
}

void AnswerInvite(bool accept)
{
    if (accept && s_invite.active)
        RoomList::Join(s_invite.id, std::wstring());

    s_invite = Invite{};
}

}
