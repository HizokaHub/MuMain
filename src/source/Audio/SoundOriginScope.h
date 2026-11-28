#pragma once

// Most sounds are played without an object (PlayBuffer(SOUND_X)). While one of these scopes is alive, such a sound belongs
// to `position`: the character, effect or network message being processed. The MOBA hearing rule can then apply to it
// (the server sends the whole arena, so a sound without a place would be heard from anywhere).
// The pointer must outlive the scope. It is compared by address with the hero's position: the hero's sounds are always heard.
namespace Audio
{
    inline thread_local const float* g_soundOrigin = nullptr;

    inline const float* CurrentSoundOrigin()
    {
        return g_soundOrigin;
    }

    class SoundOriginScope
    {
    public:
        // onlyIfNone: keep the origin of an outer scope when there is one.
        explicit SoundOriginScope(const float* position, bool onlyIfNone = false)
            : m_previous(g_soundOrigin)
        {
            if (!onlyIfNone || g_soundOrigin == nullptr)
                g_soundOrigin = position;
        }

        ~SoundOriginScope()
        {
            g_soundOrigin = m_previous;
        }

        SoundOriginScope(const SoundOriginScope&) = delete;
        SoundOriginScope& operator=(const SoundOriginScope&) = delete;

    private:
        const float* m_previous;
    };
}
