#pragma once

#include <vector>

// MOBA golden invasion: the "city" of each team is the circle around its nexus which holds its first three turrets. While golden monsters are inside it, the
// scenery of the circle (houses, walls, trees...) is set on fire. The server sends C1 D5 2F count [team x y radius burning] about once a second.
namespace World::Moba::CityFire
{
    struct City
    {
        int team = 0;
        int x = 0;        // tile of the nexus
        int y = 0;
        int radius = 0;   // tiles
        bool burning = false;
    };

    // Replaces the cities with what the server sent.
    void Set(const std::vector<City>& cities);

    // Called every frame: puts fire on the scenery of the burning cities.
    void Update();
}
