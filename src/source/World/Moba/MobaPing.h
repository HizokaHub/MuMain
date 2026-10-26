#pragma once

#include <string>
#include <vector>

// MOBA pings, the League of Legends way: hold G or Alt and click (a plain ping), hold V or Ctrl and click (caution); keep the button down and a wheel of
// nine pings opens around the click: drag toward one and let go (the centre is caution, the right button closes it). The ping goes to the team only.
//   wheel (clockwise from the top): retreat, push, on my way, all-in, assist me, need vision, enemy missing, enemy vision.
// The client sends C1 07 D5 35 kind tileX tileY; the server checks it (at most 5 pings in 5 seconds, then 6 seconds locked out) and tells the team
// C1 len D5 33 kind team x y keyLo keyHi nameLen name(UTF-8). The marks are drawn by CMobaPingHud (ground) and the minimap markers (minimap).
// This module holds the data and the rules; it draws nothing.
namespace World::Moba::Ping
{
    // As MobaPingKind on the server (never renumber, only append).
    enum Kind : unsigned char
    {
        KIND_NONE = 0,
        KIND_GENERIC = 1, KIND_CAUTION, KIND_RETREAT, KIND_PUSH, KIND_ON_MY_WAY, KIND_ALL_IN, KIND_ASSIST, KIND_NEED_VISION,
        KIND_ENEMY_MISSING, KIND_ENEMY_VISION,
        KIND_COUNT = KIND_ENEMY_VISION,
    };

    struct Mark
    {
        Kind kind = KIND_NONE;
        int team = 0;
        int tileX = 0;
        int tileY = 0;
        std::wstring sender;
        double atMs = 0.0;     // WorldTime when it arrived
    };

    // How long a mark stays on the ground and on the minimap.
    constexpr double GROUND_MS = 2500.0;
    constexpr double MINIMAP_MS = 5000.0;

    // Handles the packet C1 len D5 33.
    void Receive(const unsigned char* ReceiveBuffer);

    // The marks that are still alive (the older ones are dropped on every call), oldest first.
    const std::vector<Mark>& Marks();

    // Sends the ping of the player.
    void Send(Kind kind, int tileX, int tileY);

    // Whether the keys G and V are taken by the pings (in the arena during a match): the hotkeys that open the guild window and the inventory must then not act on
    // them (the inventory stays on I).
    bool KeysReserved();

    // Whether the player holds a ping key (G, V, Alt or Ctrl) and the kind a plain click with it makes: G and Alt = generic, V and Ctrl = caution.
    bool ModifierHeld();
    Kind DefaultKind();

    // The ping of the wheel the cursor points at: dx, dy is the cursor minus the centre of the wheel, in pixels square on screen (y down).
    // Inside the dead zone it is caution (the centre of the wheel).
    Kind WheelKind(float dx, float dy);
    constexpr float WHEEL_DEAD_ZONE = 16.f;

    // The Spanish name of a ping (the label of the wheel).
    const wchar_t* NameOf(Kind kind);

    // Sounds: one per kind (heard when a ping arrives), the wheel opening, the wheel changing slot and a refused ping.
    void PlayKindSound(Kind kind);
    void PlayWheelOpen();
    void PlayWheelTick();
}
