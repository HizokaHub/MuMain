#pragma once

// Drawing helpers shared by the MOBA queue popups (ready check, queue panel): flat rectangles, frames and centered text
// in the 640x480 reference screen, and the mouse test against a rectangle.
namespace UI::Moba::Draw
{
    struct Rect
    {
        int x, y, w, h;
    };

    // The fonts of the MOBA popups: a larger body font, its bold variant and a big title font. They follow the size of the
    // UI fonts (so the window resolution is respected) and are rebuilt when it changes.
    HFONT Body();
    HFONT Bold();
    HFONT Title();

    // The height of a line of the body font, in reference px (to centre a text field inside its frame).
    int BodyLineHeight();

    // A small box with a text next to the mouse (the name of what the mouse is over); draw it last.
    void Tooltip(const wchar_t* text);

    // Whether the mouse is inside the rectangle.
    bool MouseInRect(const Rect& r);

    // Fills the rectangle with a flat color (the color state is reset by EndRenderColor, which the caller calls after a batch).
    void FillRect(const Rect& r, float red, float green, float blue, float alpha);

    // Draws a one-pixel frame around the rectangle (inside it).
    void FrameRect(const Rect& r, float red, float green, float blue, float alpha);

    // Draws a text line centered in a box of the given width; sort = centered, left or right.
    void PutText(HFONT font, int x, int y, int width, const wchar_t* text, BYTE red, BYTE green, BYTE blue, int sort);

    // Same, centered (the common case).
    void PutTextCentered(HFONT font, int x, int y, int width, const wchar_t* text, BYTE red, BYTE green, BYTE blue);

    // Same, left aligned.
    void PutTextLeft(HFONT font, int x, int y, int width, const wchar_t* text, BYTE red, BYTE green, BYTE blue);

    // Call when a popup used a click: the popup may disappear under the cursor, and the press still held must not reach the world.
    void SwallowHeldClick();

    // Whether the press of a click used by a popup is still held (the popups return "mouse used" while it is).
    bool HeldClickSwallowed();

    // Loads the atlas of the class portraits (Data\Interface\MobaPortraits.tga, also used by the minimap) into its slot; call it when a panel with portraits opens.
    void LoadPortraits();

    // Draws the portrait of a base class (0 Dark Wizard .. 6 Rage Fighter) in a frame coloured by team (blue or red).
    void Portrait(const Rect& frame, int baseClass, bool blue);

    // A button: a filled rectangle (green when positive, red otherwise) with a gold frame and a centered label.
    void Button(const Rect& r, const wchar_t* label, bool positive);
}
