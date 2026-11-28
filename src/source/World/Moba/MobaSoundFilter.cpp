#include "stdafx.h"
#include "World/Moba/MobaSoundFilter.h"

#include "Audio/SoundOriginScope.h"
#include "World/Moba/MobaFog.h"

#include <cstdlib>
#include <fstream>

extern double WorldTime;

namespace World::Moba::SoundFilter
{

namespace
{
    constexpr const char* LOG_PATH_VARIABLE = "MOBA_SOUND_LOG";
    constexpr float NO_DISTANCE = -1.f;

    std::ofstream OpenLog()
    {
        std::ofstream log;
        const char* path = std::getenv(LOG_PATH_VARIABLE);
        if (path != nullptr && *path != '\0')
        {
            log.open(path, std::ios::app);
            log << "ms;sound;source;tilesFromListener;played\n";
        }
        return log;
    }

    void Trace(int bufferId, const char* source, const float* position, bool played)
    {
        static std::ofstream s_log = OpenLog();
        if (!s_log.is_open() || !Fog::IsActive())
            return;

        const float tiles = position != nullptr ? Fog::TilesFromListener(position) : NO_DISTANCE;
        s_log << static_cast<long long>(WorldTime) << ';' << bufferId << ';' << source << ';' << tiles << ';'
              << (played ? 1 : 0) << std::endl;
    }
}

bool Allows(int bufferId, const float* objectPosition)
{
    const float* position = objectPosition != nullptr ? objectPosition : Audio::CurrentSoundOrigin();
    const bool allowed = Fog::IsSoundAudible(bufferId, position);

    const char* source = objectPosition != nullptr ? "object" : (position != nullptr ? "origin" : "none");
    Trace(bufferId, source, position, allowed);
    return allowed;
}

}
