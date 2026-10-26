#pragma once

#include <vector>

// MOBA damage direction: where the damage the champion took came from. The server sends C1 08 D5 34 attackerX attackerY percent flags whenever the
// champion is hit (at most every 120 ms); the damage HUD draws a red wedge around the champion that points at the attacker. This module holds the latest hits.
namespace World::Moba::DamageDir
{
    struct Hit
    {
        int tileX = 0;           // the attacker
        int tileY = 0;
        int percent = 0;         // damage taken since the last notice, in percent of the maximum life (1..100)
        bool noDirection = false;   // poison, burning...: the damage has no source to point at
        bool shieldOnly = false;    // only the shield took it
        double atMs = 0.0;       // WorldTime when it arrived
    };

    // How long a hit stays on the screen.
    constexpr double HIT_MS = 1300.0;

    // Handles the packet C1 08 D5 34.
    void Receive(const unsigned char* ReceiveBuffer);

    // The hits that are still showing, oldest first (the older ones are dropped on every call).
    const std::vector<Hit>& Hits();
}
