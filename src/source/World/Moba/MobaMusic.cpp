#include "stdafx.h"
#include "World/Moba/MobaMusic.h"

#include "App/Platform/Windows/Winmain.h"     // PlayMp3, StopMp3, IsEndMp3
#include "Audio/AudioPlayer.h"
#include "Network/Server/WSclient.h"           // g_MobaLevel
#include "World/MapInfra/MapManager.h"         // gMapManager, WD_200_MOBA_ARENA
#include "World/Moba/MobaAnnouncer.h"

#include <cstring>

extern double WorldTime;

namespace World::Moba::Music
{

namespace
{
    enum Phase
    {
        PHASE_NONE,        // not in the arena
        PHASE_PREP,
        PHASE_EARLY,
        PHASE_MID,
        PHASE_GOLDEN,
        PHASE_ENDED,
        PHASE_COUNT
    };

    constexpr const char* TRACK[PHASE_COUNT] = {
        nullptr,
        "data\\music\\moba_prep.ogg",
        "data\\music\\moba_early.ogg",
        "data\\music\\moba_mid.ogg",
        "data\\music\\moba_golden.ogg",
        nullptr,
    };
    constexpr const char* JINGLE_VICTORY = "data\\sound\\w35\\CW_win.wav";
    constexpr const char* JINGLE_DEFEAT = "data\\sound\\w35\\CW_lose.wav";

    constexpr double MID_AFTER_MS = 3.0 * 60.0 * 1000.0;   // the battle music starts this long after the match start
    constexpr int FADE_MS = 1500;                            // fade out of the track that leaves
    constexpr int FADE_FAST_MS = 400;                        // before a jingle
    constexpr double FADE_MARGIN_MS = 100.0;                 // wait a little longer than the fade before starting the next track

    Phase s_phase = PHASE_NONE;
    const char* s_playing = nullptr;     // the track (or jingle) loaded in the player
    const char* s_next = nullptr;        // the track to start when the fade ends
    double s_startNextAtMs = 0.0;
    bool s_loop = false;
    double s_matchStartMs = 0.0;

    void Start(const char* path, bool loop)
    {
        if (s_playing != nullptr)
            StopMp3(s_playing);

        PlayMp3(path);
        s_playing = path;
        s_loop = loop;
        s_next = nullptr;
    }

    // Changes what is playing: fades the current track out and starts <path> when the fade ends.
    void Change(const char* path, bool loop, int fadeMs)
    {
        if (s_playing != nullptr && std::strcmp(s_playing, path) == 0 && loop == s_loop)
            return;

        if (s_playing == nullptr)
        {
            Start(path, loop);
            return;
        }

        AudioPlayer::FadeOutMusic(fadeMs);
        s_next = path;
        s_loop = loop;
        s_startNextAtMs = WorldTime + fadeMs + FADE_MARGIN_MS;
    }

    void SetPhase(Phase phase)
    {
        if (phase == s_phase)
            return;

        s_phase = phase;
        if (TRACK[phase] != nullptr)
            Change(TRACK[phase], true, FADE_MS);
    }

    void Leave()
    {
        if (s_playing != nullptr)
            StopMp3(s_playing);

        s_phase = PHASE_NONE;
        s_playing = nullptr;
        s_next = nullptr;
        s_loop = false;
    }

    void FinishMatch(bool victory)
    {
        s_phase = PHASE_ENDED;
        Change(victory ? JINGLE_VICTORY : JINGLE_DEFEAT, false, FADE_FAST_MS);
    }

    void BeginMatch()
    {
        s_matchStartMs = WorldTime;
        SetPhase(PHASE_EARLY);
    }
}

void OnAnnouncement(int kind)
{
    if (s_phase == PHASE_NONE || s_phase == PHASE_ENDED)
        return;

    switch (kind)
    {
    case Announcer::KIND_MATCH_START:
        BeginMatch();
        break;
    case Announcer::KIND_GOLDEN_FLYOVER:
    case Announcer::KIND_GOLDEN_INVADERS:
        SetPhase(PHASE_GOLDEN);
        break;
    case Announcer::KIND_VICTORY:
        FinishMatch(true);
        break;
    case Announcer::KIND_DEFEAT:
        FinishMatch(false);
        break;
    default:
        // Any other event means the match is under way (a client that joined late never saw the start).
        if (s_phase == PHASE_PREP)
            BeginMatch();
        break;
    }
}

void Update()
{
    const bool inArena = gMapManager.WorldActive == WD_200_MOBA_ARENA && g_MobaLevel > 0;
    if (!inArena)
    {
        if (s_phase != PHASE_NONE)
            Leave();
        return;
    }

    if (s_phase == PHASE_NONE)
        SetPhase(PHASE_PREP);

    if (s_phase == PHASE_EARLY && WorldTime - s_matchStartMs >= MID_AFTER_MS)
        SetPhase(PHASE_MID);

    if (s_next != nullptr)
    {
        if (WorldTime >= s_startNextAtMs)
            Start(s_next, s_loop);
        return;
    }

    // A track that ended starts again (the player plays a track once); a jingle does not.
    if (s_loop && s_playing != nullptr && IsEndMp3())
        Start(s_playing, true);
}

}
