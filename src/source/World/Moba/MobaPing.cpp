#include "stdafx.h"
#include "World/Moba/MobaPing.h"

#include "Audio/DSPlaySound.h"
#include "Data/Translation/MultiLanguage.h"   // CMultiLanguage::ConvertFromUtf8
#include "Network/Server/WSclient.h"           // g_MyMobaTeam
#include "World/MapInfra/MapManager.h"         // gMapManager, WD_200_MOBA_ARENA

#include <algorithm>
#include <cmath>

extern double WorldTime;

namespace World::Moba::Ping
{

namespace
{
    // Sound files of Data\Sound\MobaPing, in the order of their buffers (SOUND_MOBA_PING_FIRST + index): the ten kinds, then the wheel.
    enum SoundIndex : int
    {
        SOUND_WHEEL_OPEN = KIND_COUNT,
        SOUND_WHEEL_TICK,
        SOUND_COUNT
    };

    constexpr const wchar_t* SOUND_FILE[SOUND_COUNT] = {
        L"generic", L"caution", L"retreat", L"push", L"onmyway", L"allin", L"assist", L"needvision", L"missing", L"enemyvision",
        L"wheel_open", L"wheel_tick",
    };
    static_assert(SOUND_COUNT <= SOUND_MOBA_PING_LAST - SOUND_MOBA_PING_FIRST + 1, "grow SOUND_MOBA_PING_LAST");

    constexpr wchar_t SOUND_FOLDER[] = L"Data\\Sound\\MobaPing\\";
    constexpr int NAME_MAX_CHARS = 24;
    constexpr size_t MARKS_MAX = 24;
    constexpr int PACKET_HEADER_BYTES = 11;
    constexpr int SEND_LENGTH = 7;
    constexpr float SECTOR_DEGREES = 45.f;
    constexpr int SECTOR_COUNT = 8;
    constexpr float RADIANS_TO_DEGREES = 57.29577951f;

    // The eight outer slots of the wheel, clockwise from the top.
    constexpr Kind WHEEL_SLOTS[SECTOR_COUNT] = {
        KIND_RETREAT, KIND_PUSH, KIND_ON_MY_WAY, KIND_ALL_IN, KIND_ASSIST, KIND_NEED_VISION, KIND_ENEMY_MISSING, KIND_ENEMY_VISION,
    };

    constexpr const wchar_t* KIND_NAME[KIND_COUNT + 1] = {
        L"", L"Se\u00f1al", L"Peligro", L"Retirada", L"Empujar", L"Voy en camino", L"Todo o nada", L"Ay\u00fadame", L"Falta visi\u00f3n",
        L"Enemigo desaparecido", L"Visi\u00f3n enemiga",
    };

    bool s_soundsLoaded = false;
    std::vector<Mark> s_marks;

    void LoadSounds()
    {
        if (s_soundsLoaded)
            return;

        for (int index = 0; index < SOUND_COUNT; ++index)
        {
            wchar_t path[96];
            swprintf(path, std::size(path), L"%s%s.wav", SOUND_FOLDER, SOUND_FILE[index]);
            LoadWaveFile(static_cast<ESound>(SOUND_MOBA_PING_FIRST + index), path, 2);
        }
        s_soundsLoaded = true;
    }

    void Play(int index)
    {
        LoadSounds();
        PlayBuffer(static_cast<ESound>(SOUND_MOBA_PING_FIRST + index));
    }

    bool Down(int virtualKey)
    {
        return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
    }
}

void Receive(const unsigned char* ReceiveBuffer)
{
    const int length = ReceiveBuffer[1];
    if (length < PACKET_HEADER_BYTES)
        return;

    const int nameLength = ReceiveBuffer[10];
    if (PACKET_HEADER_BYTES + nameLength > length)
        return;

    const unsigned char kind = ReceiveBuffer[4];
    if (kind < KIND_GENERIC || kind > KIND_COUNT)
        return;

    wchar_t name[NAME_MAX_CHARS + 1] = {};
    char utf8[NAME_MAX_CHARS * 4 + 1] = {};
    memcpy(utf8, ReceiveBuffer + PACKET_HEADER_BYTES, std::min<int>(nameLength, sizeof(utf8) - 1));
    CMultiLanguage::ConvertFromUtf8(name, utf8, std::min<int>(nameLength, NAME_MAX_CHARS * 4));

    Mark mark;
    mark.kind = static_cast<Kind>(kind);
    mark.team = ReceiveBuffer[5];
    mark.tileX = ReceiveBuffer[6];
    mark.tileY = ReceiveBuffer[7];
    mark.sender = name;
    mark.atMs = WorldTime;
    s_marks.push_back(std::move(mark));
    if (s_marks.size() > MARKS_MAX)
        s_marks.erase(s_marks.begin());

    PlayKindSound(static_cast<Kind>(kind));
}

const std::vector<Mark>& Marks()
{
    s_marks.erase(std::remove_if(s_marks.begin(), s_marks.end(), [](const Mark& mark) { return WorldTime - mark.atMs > MINIMAP_MS; }), s_marks.end());
    return s_marks;
}

void Send(Kind kind, int tileX, int tileY)
{
    if (kind < KIND_GENERIC || kind > KIND_COUNT || tileX < 0 || tileX > 255 || tileY < 0 || tileY > 255)
        return;
    if (SocketClient == nullptr || !SocketClient->IsConnected())
        return;

    BYTE buf[SEND_LENGTH];
    buf[0] = 0xC1;
    buf[1] = SEND_LENGTH;
    buf[2] = 0xD5;
    buf[3] = 0x35;
    buf[4] = static_cast<BYTE>(kind);
    buf[5] = static_cast<BYTE>(tileX);
    buf[6] = static_cast<BYTE>(tileY);
    SocketClient->Send(buf, SEND_LENGTH);
}

bool KeysReserved()
{
    return gMapManager.WorldActive == WD_200_MOBA_ARENA && g_MobaLevel > 0;
}

bool ModifierHeld()
{
    return Down('G') || Down('V') || Down(VK_MENU) || Down(VK_CONTROL);
}

Kind DefaultKind()
{
    // V and Ctrl mean caution; G and Alt a plain ping (when both groups are held, caution wins: it is the safer message).
    if (Down('V') || Down(VK_CONTROL))
        return KIND_CAUTION;
    return KIND_GENERIC;
}

Kind WheelKind(float dx, float dy)
{
    if (dx * dx + dy * dy < WHEEL_DEAD_ZONE * WHEEL_DEAD_ZONE)
        return KIND_CAUTION;

    // Degrees clockwise from the top (0 = up, 90 = right).
    float degrees = std::atan2(dx, -dy) * RADIANS_TO_DEGREES;
    if (degrees < 0.f)
        degrees += 360.f;
    const int sector = static_cast<int>(std::floor((degrees + SECTOR_DEGREES * 0.5f) / SECTOR_DEGREES)) % SECTOR_COUNT;
    return WHEEL_SLOTS[sector];
}

const wchar_t* NameOf(Kind kind)
{
    return kind >= KIND_GENERIC && kind <= KIND_COUNT ? KIND_NAME[kind] : KIND_NAME[0];
}

void PlayKindSound(Kind kind)
{
    if (kind >= KIND_GENERIC && kind <= KIND_COUNT)
        Play(kind - 1);
}

void PlayWheelOpen()
{
    Play(SOUND_WHEEL_OPEN);
}

void PlayWheelTick()
{
    Play(SOUND_WHEEL_TICK);
}

}
