#pragma once

// Draws the icon of a skill by its number (the same atlas lookup the skill bar and the MU Helper use).
namespace UI::Skills::Icon
{
    // Draws the icon of the skill at (x, y) in the 640x480 reference screen, scaled to width x height.
    void Render(int skill, float x, float y, float width, float height);
}
