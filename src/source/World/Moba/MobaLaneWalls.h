#pragma once

// MOBA lane walls: in the formats which play the middle lane only (1v1, ARAM) the arena is turned into a corridor by walls along
// both sides of the middle lane and across both ends. The server sends the wall rectangles every second:
// C1 len D5 17  count  count * (x y width height)  (tile units). The client turns the tiles into walls on its own terrain (so walking
// stops at the wall, no rubber-banding) and draws a wall of energy along each rectangle.
//
// Everything is a no-op outside the MOBA arena; the walls drop by themselves if the server stops refreshing them.
namespace World::Moba::LaneWalls
{
    // Handles the packet C1 len D5 17.
    void Receive(const unsigned char* ReceiveBuffer);

    // Once per frame: keeps the wall tiles solid, and drops the walls if they expired or the map changed.
    void Update();

    // Draws the energy walls in the 3D pass, after the effects.
    void Render();

    // Whether the walls are up.
    bool IsActive();
}
