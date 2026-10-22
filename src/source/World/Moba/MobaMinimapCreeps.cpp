#include "stdafx.h"
#include "World/Moba/MobaMinimapCreeps.h"

extern double WorldTime;

namespace World::Moba::MinimapCreeps
{

namespace
{
    // Without news for this long the dots are dropped (the match ended or the packets stopped).
    constexpr double STALE_AFTER_MS = 4000.0;

    std::vector<Dot> s_dots;
    double s_updatedMs = 0.0;
}

void Set(std::vector<Dot> dots)
{
    s_dots = std::move(dots);
    s_updatedMs = WorldTime;
}

const std::vector<Dot>& Get()
{
    static const std::vector<Dot> s_none;
    return WorldTime - s_updatedMs > STALE_AFTER_MS ? s_none : s_dots;
}

}
