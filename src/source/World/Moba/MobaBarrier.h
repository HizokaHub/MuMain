#pragma once

// MOBA pre-match barrier: for the seconds before a queued match starts the champions are held inside a circle
// around their base. The server blocks the ring on its terrain (so a walk through it fails) and sends the state
// every second (C1 0A D5 13 active radius blueX blueY redX redY). The client mirrors it: the ring cells become
// walls here too (so walking stops at the barrier, no rubber-banding) and a wall of blue energy is drawn on them.
//
// Everything is a no-op outside the MOBA arena; the barrier drops by itself if the server stops refreshing it.
namespace World::Moba::Barrier
{
    // Handles the packet C1 0A D5 13.
    void Receive(const unsigned char* ReceiveBuffer);

    // Once per frame: drops the barrier if it expired or the map changed.
    void Update();

    // Draws the energy wall (a closed ring around each base, bright at the foot and fading upward) in the 3D pass,
    // after the effects.
    void Render();

    // Whether the barrier is up.
    bool IsActive();
}
