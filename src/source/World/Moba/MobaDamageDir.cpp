#include "stdafx.h"
#include "World/Moba/MobaDamageDir.h"

#include <algorithm>

extern double WorldTime;

namespace World::Moba::DamageDir
{

namespace
{
    constexpr int PACKET_BYTES = 8;
    constexpr size_t HITS_MAX = 8;
    constexpr unsigned char FLAG_NO_DIRECTION = 1;
    constexpr unsigned char FLAG_SHIELD_ONLY = 2;

    std::vector<Hit> s_hits;
}

void Receive(const unsigned char* ReceiveBuffer)
{
    if (ReceiveBuffer[1] < PACKET_BYTES)
        return;

    Hit hit;
    hit.tileX = ReceiveBuffer[4];
    hit.tileY = ReceiveBuffer[5];
    hit.percent = std::clamp<int>(ReceiveBuffer[6], 1, 100);
    hit.noDirection = (ReceiveBuffer[7] & FLAG_NO_DIRECTION) != 0;
    hit.shieldOnly = (ReceiveBuffer[7] & FLAG_SHIELD_ONLY) != 0;
    hit.atMs = WorldTime;
    s_hits.push_back(hit);
    if (s_hits.size() > HITS_MAX)
        s_hits.erase(s_hits.begin());
}

const std::vector<Hit>& Hits()
{
    s_hits.erase(std::remove_if(s_hits.begin(), s_hits.end(), [](const Hit& hit) { return WorldTime - hit.atMs > HIT_MS; }), s_hits.end());
    return s_hits;
}

}
