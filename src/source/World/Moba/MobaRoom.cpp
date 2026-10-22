#include "stdafx.h"
#include "World/Moba/MobaRoom.h"

#include "Network/Server/WSclient.h"  // SendMobaRoomAction

namespace World::Moba::Room
{

namespace
{
    State s_state;
    unsigned int s_version = 1;
}

void Show(State state)
{
    s_state = std::move(state);
    ++s_version;
}

unsigned int Version()
{
    return s_version;
}

const State& Get()
{
    return s_state;
}

void Request(Action action)
{
    if (!s_state.present)
        return;

    SendMobaRoomAction(static_cast<unsigned char>(action));
}

}
