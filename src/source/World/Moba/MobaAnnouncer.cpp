#include "stdafx.h"
#include "World/Moba/MobaAnnouncer.h"

#include "Audio/DSPlaySound.h"
#include "Data/Translation/MultiLanguage.h"   // CMultiLanguage::ConvertFromUtf8
#include "Network/Server/WSclient.h"           // g_MyMobaTeam
#include "UI/NewUI/NewUISystem.h"               // g_pOption
#include "World/Moba/MobaBanner.h"
#include "World/Moba/MobaMusic.h"

#include <algorithm>
#include <cstdio>

extern double WorldTime;

namespace World::Moba::Announcer
{

namespace
{
    // The voice lines, in the order of their sound buffers (SOUND_MOBA_ANN_FIRST + index).
    enum Voice : int
    {
        VOICE_NONE = -1,
        VOICE_FIRST_BLOOD, VOICE_DOUBLE, VOICE_TRIPLE, VOICE_QUADRA, VOICE_PENTA, VOICE_ACE, VOICE_ACE_LOST, VOICE_SHUTDOWN,
        VOICE_SPREE, VOICE_DOMINATING, VOICE_UNSTOPPABLE, VOICE_LEGENDARY, VOICE_TURRET_ALLY, VOICE_TURRET_ENEMY,
        VOICE_NEXUS_ATTACK, VOICE_MATCH_START, VOICE_VICTORY, VOICE_DEFEAT, VOICE_GOLDEN_FLYOVER, VOICE_GOLDEN_INVADERS,
        VOICE_DRAGON_ALLY, VOICE_DRAGON_ENEMY,
        VOICE_INHIBITOR_ALLY_LOST, VOICE_INHIBITOR_ENEMY_DOWN, VOICE_INHIBITOR_ALLY_BACK, VOICE_INHIBITOR_ENEMY_BACK, VOICE_SUPER_ALLY, VOICE_SUPER_ENEMY,
        VOICE_COUNT
    };

    constexpr const wchar_t* VOICE_FILE[VOICE_COUNT] = {
        L"first_blood", L"double", L"triple", L"quadra", L"penta", L"ace", L"ace_lost", L"shutdown",
        L"spree", L"dominating", L"unstoppable", L"legendary", L"turret_ally", L"turret_enemy",
        L"nexus_attack", L"match_start", L"victory", L"defeat", L"golden_flyover", L"golden_invaders",
        L"dragon_ally", L"dragon_enemy",
        L"inhibitor_ally_lost", L"inhibitor_enemy_down", L"inhibitor_ally_back", L"inhibitor_enemy_back", L"super_ally", L"super_enemy",
    };
    static_assert(VOICE_COUNT <= SOUND_MOBA_ANN_LAST - SOUND_MOBA_ANN_FIRST + 1, "grow SOUND_MOBA_ANN_LAST");

    // Higher interrupts lower; equal or lower waits.
    constexpr int VOICE_PRIORITY[VOICE_COUNT] = {
        60, 50, 80, 90, 100, 85, 85, 55,
        40, 45, 47, 49, 30, 30,
        70, 20, 100, 100, 35, 35,
        45, 45,
        75, 65, 55, 40, 65, 72,
    };

    constexpr wchar_t SOUND_FOLDER[] = L"Data\\Sound\\MobaAnn\\";
    constexpr int BANNER_MS = 3200;
    constexpr int FEED_MAX_LINES = 6;
    constexpr double WAITING_STALE_MS = 4000.0;
    constexpr size_t WAITING_MAX = 3;
    constexpr double DEFAULT_VOICE_MS = 2500.0;
    constexpr int NAME_MAX_CHARS = 32;
    constexpr int WAV_HEADER_MIN_BYTES = 44;

    struct Waiting
    {
        Voice voice;
        double atMs;
    };

    struct Cue
    {
        Voice voice = VOICE_NONE;
        std::wstring banner;
        std::wstring feedSuffix;   // appended to the kill feed line ("(Doble muerte)")
        bool feed = false;
    };

    bool s_loaded = false;
    double s_durationMs[VOICE_COUNT] = {};
    Voice s_playing = VOICE_NONE;
    double s_busyUntilMs = 0.0;
    std::vector<Waiting> s_waiting;
    std::vector<FeedLine> s_feed;

    ESound SoundOf(Voice voice) { return static_cast<ESound>(SOUND_MOBA_ANN_FIRST + voice); }

    // Length of a PCM WAV file in milliseconds (byte rate and size of the data chunk), or the default when it cannot be read.
    double WavDurationMs(const wchar_t* path)
    {
        FILE* file = _wfopen(path, L"rb");
        if (file == nullptr)
            return DEFAULT_VOICE_MS;

        unsigned char head[WAV_HEADER_MIN_BYTES] = {};
        const size_t read = fread(head, 1, sizeof(head), file);
        fclose(file);
        if (read < sizeof(head))
            return DEFAULT_VOICE_MS;

        const unsigned int byteRate = head[28] | (head[29] << 8) | (head[30] << 16) | (static_cast<unsigned int>(head[31]) << 24);
        const unsigned int dataBytes = head[40] | (head[41] << 8) | (head[42] << 16) | (static_cast<unsigned int>(head[43]) << 24);
        return byteRate > 0 ? 1000.0 * dataBytes / byteRate : DEFAULT_VOICE_MS;
    }

    void LoadAll()
    {
        if (s_loaded)
            return;

        for (int voice = 0; voice < VOICE_COUNT; ++voice)
        {
            wchar_t path[96];
            swprintf(path, std::size(path), L"%s%s.wav", SOUND_FOLDER, VOICE_FILE[voice]);
            LoadWaveFile(SoundOf(static_cast<Voice>(voice)), path, 1);
            s_durationMs[voice] = WavDurationMs(path);
        }
        s_loaded = true;
    }

    void StartVoice(Voice voice)
    {
        if (s_playing != VOICE_NONE && WorldTime < s_busyUntilMs)
            StopBuffer(SoundOf(s_playing), TRUE);

        SetCueVolume(SoundOf(voice), g_pOption->GetVolumeLevel());
        PlayBuffer(SoundOf(voice));
        s_playing = voice;
        s_busyUntilMs = WorldTime + s_durationMs[voice];
    }

    void Say(Voice voice)
    {
        if (voice == VOICE_NONE)
            return;

        LoadAll();
        const bool busy = s_playing != VOICE_NONE && WorldTime < s_busyUntilMs;
        if (!busy || VOICE_PRIORITY[voice] > VOICE_PRIORITY[s_playing])
        {
            StartVoice(voice);
            return;
        }

        s_waiting.push_back({ voice, WorldTime });
        if (s_waiting.size() > WAITING_MAX)
            s_waiting.erase(std::min_element(s_waiting.begin(), s_waiting.end(), [](const Waiting& a, const Waiting& b) { return VOICE_PRIORITY[a.voice] < VOICE_PRIORITY[b.voice]; }));
    }

    std::wstring Utf8Name(const unsigned char* bytes, int length)
    {
        wchar_t buffer[NAME_MAX_CHARS + 1] = {};
        char utf8[NAME_MAX_CHARS * 4 + 1] = {};
        memcpy(utf8, bytes, std::min<int>(length, sizeof(utf8) - 1));
        CMultiLanguage::ConvertFromUtf8(buffer, utf8, std::min<int>(length, NAME_MAX_CHARS * 4));
        return buffer;
    }

    // What a kind of event is, as heard and seen by a player of the team <mine> (true when the event is about the player's own team).
    Cue CueOf(Kind kind, bool mine, const std::wstring& killer)
    {
        const std::wstring by = killer.empty() ? std::wstring() : killer + L": ";
        switch (kind)
        {
        case KIND_KILL:            return { VOICE_NONE, L"", L"", true };
        case KIND_FIRST_BLOOD:     return { VOICE_FIRST_BLOOD, by + L"\u00a1PRIMERA SANGRE!", L" (Primera sangre)", true };
        case KIND_DOUBLE:          return { VOICE_DOUBLE, by + L"\u00a1DOBLE MUERTE!", L" (Doble muerte)", true };
        case KIND_TRIPLE:          return { VOICE_TRIPLE, by + L"\u00a1TRIPLE MUERTE!", L" (Triple muerte)", true };
        case KIND_QUADRA:          return { VOICE_QUADRA, by + L"\u00a1CU\u00c1DRUPLE MUERTE!", L" (Cu\u00e1druple muerte)", true };
        case KIND_PENTA:           return { VOICE_PENTA, by + L"\u00a1PENTA MUERTE!", L" (Penta muerte)", true };
        case KIND_ACE:             return { mine ? VOICE_ACE : VOICE_ACE_LOST, mine ? L"\u00a1MASACRE!" : L"EQUIPO ANIQUILADO", L"", false };
        case KIND_SHUTDOWN:        return { VOICE_SHUTDOWN, by + L"\u00a1RACHA CORTADA!", L" (Racha cortada)", true };
        case KIND_SPREE:           return { VOICE_SPREE, by + L"RACHA DE MUERTES", L" (Racha de muertes)", true };
        case KIND_DOMINATING:      return { VOICE_DOMINATING, by + L"\u00a1DOMINANDO!", L" (Dominando)", true };
        case KIND_UNSTOPPABLE:     return { VOICE_UNSTOPPABLE, by + L"\u00a1IMPARABLE!", L" (Imparable)", true };
        case KIND_LEGENDARY:       return { VOICE_LEGENDARY, by + L"\u00a1LEGENDARIO!", L" (Legendario)", true };
        case KIND_TURRET:          return { mine ? VOICE_TURRET_ALLY : VOICE_TURRET_ENEMY, mine ? L"HAS PERDIDO UNA TORRE" : L"TORRE ENEMIGA DESTRUIDA", L"", false };
        case KIND_NEXUS_ATTACK:    return mine ? Cue{ VOICE_NEXUS_ATTACK, L"\u00a1TU NEXO EST\u00c1 BAJO ATAQUE!", L"", false } : Cue{};
        case KIND_MATCH_START:     return { VOICE_MATCH_START, L"", L"", false };
        case KIND_VICTORY:         return { VOICE_VICTORY, L"", L"", false };
        case KIND_DEFEAT:          return { VOICE_DEFEAT, L"", L"", false };
        case KIND_GOLDEN_FLYOVER:  return { VOICE_GOLDEN_FLYOVER, L"", L"", false };
        case KIND_GOLDEN_INVADERS: return { VOICE_GOLDEN_INVADERS, L"", L"", false };
        case KIND_DRAGON_SLAIN:    return { mine ? VOICE_DRAGON_ALLY : VOICE_DRAGON_ENEMY, L"", L"", false };
        case KIND_INHIBITOR_DESTROYED: return { mine ? VOICE_INHIBITOR_ALLY_LOST : VOICE_INHIBITOR_ENEMY_DOWN, mine ? L"TU INHIBIDOR HA CA\u00cdDO" : L"INHIBIDOR ENEMIGO DESTRUIDO", L"", false };
        case KIND_INHIBITOR_RESTORED:  return { mine ? VOICE_INHIBITOR_ALLY_BACK : VOICE_INHIBITOR_ENEMY_BACK, L"", L"", false };
        case KIND_SUPER_MINIONS:       return { mine ? VOICE_SUPER_ALLY : VOICE_SUPER_ENEMY, mine ? L"\u00a1SUPERMINIONS EN CAMINO!" : L"\u00a1SUPERMINIONS ENEMIGOS!", L"", false };
        default:                   return {};
        }
    }

    void AddFeedLine(std::wstring text, int team)
    {
        s_feed.push_back({ std::move(text), team, WorldTime });
        if (static_cast<int>(s_feed.size()) > FEED_MAX_LINES)
            s_feed.erase(s_feed.begin());
    }

    // The kill feed line of an event: "killer mato a victim (Doble muerte)", or the plain text for structures and objectives.
    void FeedFor(Kind kind, int team, const std::wstring& killer, const std::wstring& victim, const Cue& cue)
    {
        if (cue.feed && !killer.empty() && !victim.empty())
        {
            AddFeedLine(killer + L" mat\u00f3 a " + victim + cue.feedSuffix, team);
            return;
        }

        if (kind == KIND_TURRET || kind == KIND_DRAGON_SLAIN || kind == KIND_ACE || kind == KIND_INHIBITOR_DESTROYED || kind == KIND_INHIBITOR_RESTORED)
        {
            const std::wstring text = kind == KIND_TURRET ? L"Cay\u00f3 una torre" : kind == KIND_ACE ? L"Equipo aniquilado" : kind == KIND_INHIBITOR_DESTROYED ? L"Cay\u00f3 un inhibidor" : kind == KIND_INHIBITOR_RESTORED ? L"Un inhibidor reapareci\u00f3" : L"Cay\u00f3 un Gran Drag\u00f3n Dorado";
            AddFeedLine(text, team);
        }
    }
}

void Receive(const unsigned char* ReceiveBuffer)
{
    constexpr int HEADER_BYTES = 7;
    const int length = ReceiveBuffer[1];
    if (length < HEADER_BYTES + 1)
        return;

    const Kind kind = static_cast<Kind>(ReceiveBuffer[4]);
    const int team = ReceiveBuffer[5];
    const int killerLength = ReceiveBuffer[6];
    if (HEADER_BYTES + killerLength + 1 > length)
        return;

    const int victimLength = ReceiveBuffer[HEADER_BYTES + killerLength];
    if (HEADER_BYTES + killerLength + 1 + victimLength > length)
        return;

    const std::wstring killer = Utf8Name(ReceiveBuffer + HEADER_BYTES, killerLength);
    const std::wstring victim = Utf8Name(ReceiveBuffer + HEADER_BYTES + killerLength + 1, victimLength);
    const Cue cue = CueOf(kind, team != 0 && team == g_MyMobaTeam, killer);
    FeedFor(kind, team, killer, victim, cue);
    if (!cue.banner.empty())
        Banner::Show(cue.banner.c_str(), BANNER_MS);
    Say(cue.voice);
    Music::OnAnnouncement(kind);
}

void Pump()
{
    if (s_waiting.empty() || WorldTime < s_busyUntilMs)
        return;

    s_waiting.erase(std::remove_if(s_waiting.begin(), s_waiting.end(), [](const Waiting& w) { return WorldTime - w.atMs > WAITING_STALE_MS; }), s_waiting.end());
    if (s_waiting.empty())
        return;

    const auto next = std::max_element(s_waiting.begin(), s_waiting.end(), [](const Waiting& a, const Waiting& b) { return VOICE_PRIORITY[a.voice] < VOICE_PRIORITY[b.voice]; });
    const Voice voice = next->voice;
    s_waiting.erase(next);
    StartVoice(voice);
}

const std::vector<FeedLine>& Feed()
{
    s_feed.erase(std::remove_if(s_feed.begin(), s_feed.end(), [](const FeedLine& line) { return WorldTime - line.atMs > FEED_LINE_MS; }), s_feed.end());
    return s_feed;
}

}
