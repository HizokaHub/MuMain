#include "stdafx.h"
#include "World/Moba/MobaRoom.h"

#include "Network/Server/WSclient.h"  // SendMobaRoomAction, SendMobaRoomTargetAction, SendMobaRoomChat
#include "World/Moba/MobaRoomList.h"

namespace World::Moba::Room
{

namespace
{
    constexpr size_t MAX_CHAT_LINES = 60;

    State s_state;
    unsigned int s_version = 1;
    std::vector<ChatLine> s_chat;
    unsigned int s_chatVersion = 1;
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

}
