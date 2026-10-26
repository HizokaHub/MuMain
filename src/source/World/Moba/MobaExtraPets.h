#pragma once

#include <utility>
#include <vector>

struct CHARACTER;

// MOBA: the pets a champion switched on from its inventory (right click) are drawn next to it all at once, apart from the equipment.
// The server sends C1 D5 2C count [keyLo keyHi maskLo maskHi] about once a second; the mask has one bit per pet, in this order:
// 0 Guardian Angel, 1 Imp, 2 Demon, 3 Spirit of Guardian, 4 Rudolf, 5 Panda, 6 Unicorn, 7 Skeleton, 8 Dark Raven.
namespace World::Moba::ExtraPets
{
    // How many pets the mask has bits for.
    constexpr int PET_COUNT = 9;

    // The mask of pets the server says the champion has on (0 when none).
    unsigned short MaskOf(unsigned short key);

    // Replaces the list of the champions which have extra pets with what the server sent.
    void SetAll(const std::vector<std::pair<unsigned short, unsigned short>>& pets);

    // Called every frame for each character of the arena: creates the pets it should have and removes the ones it should not.
    void Apply(CHARACTER* c);
}
