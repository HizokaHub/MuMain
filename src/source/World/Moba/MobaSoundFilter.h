#pragma once

// Decides whether a sound is heard in a MOBA match (the rule itself is Fog::IsSoundAudible).
namespace World::Moba::SoundFilter
{
    // objectPosition: the position of the object the sound is played on, or null. Without one, the position of the
    // surrounding Audio::SoundOriginScope is used; a sound with neither (interface, announcer) is always heard.
    // With the environment variable MOBA_SOUND_LOG set to a file path, every sound of a match is written to that file.
    bool Allows(int bufferId, const float* objectPosition);
}
