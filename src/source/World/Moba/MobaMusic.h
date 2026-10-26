#pragma once

// MOBA background music: in the arena the music follows the phase of the match, with a fade between tracks and a loop on each one.
//   preparation (countdown before the match)  ->  early game (from the start)  ->  battle (10 minutes after the start)
//   ->  golden invasion (from the dragons' fly-over to the end)  ->  victory / defeat jingle (the Crywolf win / lose music) and silence.
// The phases come from the announcer's events (World::Moba::Announcer): the match start, the golden invasion and the end. A client that joins a match
// already running has no start event; the first event of play (a kill, a fallen turret...) moves it from preparation to the early game.
// The tracks are the OGG files of Data\Music (moba_prep, moba_early, moba_mid, moba_golden); the player's music volume and the music on / off option apply.
namespace World::Moba::Music
{
    // The announcer told an event (kind as World::Moba::Announcer::Kind).
    void OnAnnouncement(int kind);

    // Called every frame from the background music manager: starts the music when the player is in the arena, loops it, changes the phase by time and
    // stops it when the player leaves.
    void Update();
}
