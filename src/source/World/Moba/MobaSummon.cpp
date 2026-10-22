#include "stdafx.h"
#include "World/Moba/MobaSummon.h"

#include "Network/Server/WSclient.h"  // SendMobaSummonControl, SendMobaSummonOrder

extern double WorldTime;

namespace World::Moba::Summon
{

namespace
{
    // Without news for this long the creature is taken as gone (the match ended or the packets stopped).
    constexpr double STALE_AFTER_MS = 4000.0;

    State s_state;
    double s_updatedMs = -1.0e9;
    bool s_localControl = false;   // set at once by the key, until the server confirms
}

void Set(State state)
{
    s_state = state;
    s_localControl = state.present && state.control;
    s_updatedMs = WorldTime;
}

const State& Get()
{
    static const State s_none;
    return (WorldTime - s_updatedMs > STALE_AFTER_MS || !s_state.present) ? s_none : s_state;
}

bool Controlling()
{
    return Get().present && s_localControl;
}

void ToggleControl()
{
    if (!Get().present)
        return;

    // X commands it; X again releases it where it stands.
    s_localControl = !s_localControl;
    SendMobaSummonControl(s_localControl ? 1 : 0);
}

void CallBack()
{
    if (!Get().present)
        return;

    s_localControl = false;
    SendMobaSummonControl(2);
}

void OrderMove(int tileX, int tileY)
{
    if (!Controlling() || tileX < 0 || tileX > 255 || tileY < 0 || tileY > 255)
        return;

    SendMobaSummonOrder(1, static_cast<unsigned char>(tileX), static_cast<unsigned char>(tileY), 0);
}

void OrderAttack(unsigned short key)
{
    if (!Controlling())
        return;

    SendMobaSummonOrder(2, 0, 0, key);
}

}
