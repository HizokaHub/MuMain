#pragma once

// MOBA fog of war on the corner minimap: a black layer whose alpha follows the fog of each tile
// (World::Moba::Fog), drawn over the arena art and under the markers, with the art's placement and rotation.
namespace UI::Minimap::MobaFog
{
    // Where the art was drawn: the arguments it was given to RenderBitmapRotate (reference px, degrees) and
    // the map opacity option.
    struct ArtPlacement
    {
        float centreX, centreY;
        float width, height;
        float rotation;
        float opacity;
    };

    // clearCells: 256 * 256 flags in TERRAIN_INDEX order (y * 256 + x), set where the art is transparent (the
    // rock outside the arena), which stays clear. clearCellsVersion must change whenever their content does.
    // No-op while the fog is off.
    void Render(const ArtPlacement& art, const unsigned char* clearCells, unsigned int clearCellsVersion);
}
