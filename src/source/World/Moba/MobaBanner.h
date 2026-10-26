#pragma once

// MOBA banner: a big gold text in the upper middle of the screen for a few seconds ("\u00a1COMIENZA LA PARTIDA!"),
// drawn with the double-size UI font and faded in and out. The server sends it with C1 len D5 14 durationMs(u16 LE) text(UTF-8).
namespace World::Moba::Banner
{
    // Handles the packet C1 len D5 14.
    void Receive(const unsigned char* ReceiveBuffer);

    // Shows a banner made by the client (the announcer), replacing the one on screen.
    void Show(const wchar_t* text, double durationMs);

    // Draws the banner, if one is showing (once per frame, in the 2D pass).
    void Render();
}
