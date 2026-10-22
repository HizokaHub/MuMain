#include "stdafx.h"
#include "World/Moba/MobaRoomList.h"

#include "Network/Server/WSclient.h"  // SendMobaRoomListRequest, SendMobaRoomCreate, SendMobaRoomJoin

namespace World::Moba::RoomList
{

namespace
{
    State s_state;
    unsigned int s_version = 1;
}

void Show(std::vector<Entry> rooms, bool open)
{
    s_state.rooms = std::move(rooms);
    if (open)
        s_state.open = true;

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

void Refresh()
{
    SendMobaRoomListRequest();
}

void Create(const std::wstring& name, int teamSize, bool midOnly, bool bots, const std::wstring& password)
{
    const unsigned char flags = static_cast<unsigned char>((midOnly ? FLAG_MID_ONLY : 0) | (bots ? FLAG_BOTS : 0));
    SendMobaRoomCreate(name, static_cast<unsigned char>(teamSize), flags, password);
}

void Join(unsigned int id, const std::wstring& password)
{
    SendMobaRoomJoin(id, password);
}

void Close()
{
    s_state = State{};
}

}
