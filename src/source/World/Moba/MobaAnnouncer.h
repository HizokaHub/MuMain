#pragma once

#include <string>
#include <vector>

// MOBA announcer: the events of the match told by the server (first blood, multi-kills, aces, shutdowns, kill streaks, turrets, the nexus under attack,
// the start and the end, the golden invasion) become a voice line, a banner and a line of the kill feed. The server sends
// C1 len D5 32  kind team killerLen killer(UTF-8) victimLen victim(UTF-8)
// (kind and team as MobaAnnouncement on the server: team 1 blue, 2 red). The voice lines are the WAV files of Data\Sound\MobaAnn; one is heard at a time,
// a more important one interrupts, the rest wait their turn. The kill feed is drawn by CMobaKillFeedHud.
namespace World::Moba::Announcer
{
    struct FeedLine
    {
        std::wstring text;
        int team = 0;          // team colour of the line (0 = neutral)
        double atMs = 0.0;     // WorldTime when it arrived
    };

    // Handles the packet C1 len D5 32.
    void Receive(const unsigned char* ReceiveBuffer);

    // Starts the next waiting voice line when the current one ended (call every frame).
    void Pump();

    // The lines of the kill feed, oldest first; the expired ones are dropped on every call.
    const std::vector<FeedLine>& Feed();

    // How long a line of the kill feed stays, and how much of it is spent fading out.
    constexpr double FEED_LINE_MS = 8000.0;
    constexpr double FEED_FADE_MS = 1500.0;
}
