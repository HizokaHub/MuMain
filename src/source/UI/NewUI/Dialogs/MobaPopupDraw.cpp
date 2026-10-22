#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"

#include "UI/NewUI/NewUICommon.h"             // CheckMouseIn
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "App/Platform/Windows/Winmain.h"     // g_hFontBold
#include "Render/Textures/ZzzOpenglUtil.h"    // RenderColor, EndRenderColor
#include "Engine/Object/ZzzInterface.h"        // FontHeight
#include "Data/GameConfig/GameConfig.h"        // the UI font face

namespace UI::Moba::Draw
{

namespace
{
    constexpr int ALPHA_FULL = 255;
    constexpr int BORDER = 1;
    constexpr int BUTTON_TEXT_DY = 7;
    constexpr float HOT_SHADE = 1.f;
    constexpr float IDLE_SHADE = 0.75f;
    constexpr float FRAME_HOT_ALPHA = 1.f;
    constexpr float FRAME_IDLE_ALPHA = 0.6f;
}

namespace
{
    bool s_swallowHeldClick = false;

    constexpr float BODY_SCALE = 1.35f;
    constexpr float TITLE_SCALE = 2.3f;
    constexpr int NOT_BUILT = -1;

    HFONT s_body = nullptr;
    HFONT s_bold = nullptr;
    HFONT s_title = nullptr;
    int s_builtForHeight = NOT_BUILT;

    HFONT MakeFont(float scale, int weight)
    {
        const std::wstring selected = GameConfig::GetInstance().GetFontSelection();
        const wchar_t* face = selected.empty() ? L"Tahoma" : selected.c_str();
        const int size = static_cast<int>((FontHeight - 1) * scale);
        return CreateFont(size, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_NATURAL_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
    }

    // The old fonts are not deleted when the resolution changes: a text field may still hold one (the loss is a few handles).
    void ForgetFonts()
    {
        s_body = nullptr;
        s_bold = nullptr;
        s_title = nullptr;
    }

    // Builds the fonts the first time and whenever the size of the UI fonts changed (a new window resolution).
    void EnsureFonts()
    {
        if (s_body != nullptr && s_builtForHeight == FontHeight)
            return;

        ForgetFonts();
        s_body = MakeFont(BODY_SCALE, FW_NORMAL);
        s_bold = MakeFont(BODY_SCALE, FW_SEMIBOLD);
        s_title = MakeFont(TITLE_SCALE, FW_SEMIBOLD);
        s_builtForHeight = FontHeight;
    }
}

int BodyLineHeight()
{
    constexpr float LINE_TO_FONT = 1.2f;   // the line of a font is a bit taller than its size
    const float px = (FontHeight - 1) * BODY_SCALE * LINE_TO_FONT;
    return (g_fScreenRate_y > 0.f) ? static_cast<int>(px / g_fScreenRate_y + 0.5f) : static_cast<int>(px);
}

void Tooltip(const wchar_t* text)
{
    constexpr int PAD = 4;
    constexpr int CURSOR_GAP = 14;
    g_pRenderText->SetFont(Body());
    SIZE size = { 0, 0 };
    GetTextExtentPoint32(g_pRenderText->GetFontDC(), text, static_cast<int>(wcslen(text)), &size);
    const int width = (g_fScreenRate_x > 0.f ? static_cast<int>(size.cx / g_fScreenRate_x) : size.cx) + 2 * PAD;
    const int height = BodyLineHeight() + 2 * PAD;
    const int x = std::clamp(static_cast<int>(MouseX) + CURSOR_GAP, 0, REFERENCE_WIDTH - width);
    const int y = std::clamp(static_cast<int>(MouseY) + CURSOR_GAP, 0, REFERENCE_HEIGHT - height);
    FillRect({ x, y, width, height }, 0.02f, 0.02f, 0.03f, 1.f);
    FrameRect({ x, y, width, height }, 0.95f, 0.80f, 0.40f, 1.f);
    EndRenderColor();
    PutTextLeft(Body(), x + PAD, y + PAD, width, text, 255, 244, 214);
}

HFONT Body()
{
    EnsureFonts();
    return s_body;
}

HFONT Bold()
{
    EnsureFonts();
    return s_bold;
}

HFONT Title()
{
    EnsureFonts();
    return s_title;
}

void SwallowHeldClick()
{
    s_swallowHeldClick = true;
}

bool HeldClickSwallowed()
{
    if (!s_swallowHeldClick)
        return false;

    if (SEASON3B::IsPress(VK_LBUTTON) || SEASON3B::IsRepeat(VK_LBUTTON))
        return true;

    s_swallowHeldClick = false;
    return false;
}

bool MouseInRect(const Rect& r)
{
    return SEASON3B::CheckMouseIn(r.x, r.y, r.w, r.h);
}

void FillRect(const Rect& r, float red, float green, float blue, float alpha)
{
    glColor4f(red, green, blue, alpha);
    RenderColor(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h), 0.f);
}

void FrameRect(const Rect& r, float red, float green, float blue, float alpha)
{
    FillRect({ r.x, r.y, r.w, BORDER }, red, green, blue, alpha);
    FillRect({ r.x, r.y + r.h - BORDER, r.w, BORDER }, red, green, blue, alpha);
    FillRect({ r.x, r.y, BORDER, r.h }, red, green, blue, alpha);
    FillRect({ r.x + r.w - BORDER, r.y, BORDER, r.h }, red, green, blue, alpha);
}

void PutText(HFONT font, int x, int y, int width, const wchar_t* text, BYTE red, BYTE green, BYTE blue, int sort)
{
    g_pRenderText->SetFont(font);
    g_pRenderText->SetBgColor(0, 0, 0, 0);
    g_pRenderText->SetTextColor(red, green, blue, ALPHA_FULL);
    g_pRenderText->RenderText(x, y, text, width, 0, sort);
}

void PutTextCentered(HFONT font, int x, int y, int width, const wchar_t* text, BYTE red, BYTE green, BYTE blue)
{
    PutText(font, x, y, width, text, red, green, blue, RT3_SORT_CENTER);
}

void PutTextLeft(HFONT font, int x, int y, int width, const wchar_t* text, BYTE red, BYTE green, BYTE blue)
{
    PutText(font, x, y, width, text, red, green, blue, RT3_SORT_LEFT);
}

void Button(const Rect& r, const wchar_t* label, bool positive)
{
    const bool hot = MouseInRect(r);
    const float shade = hot ? HOT_SHADE : IDLE_SHADE;
    if (positive)
        FillRect(r, 0.12f * shade, 0.40f * shade, 0.25f * shade, 0.95f);
    else
        FillRect(r, 0.30f * shade, 0.12f * shade, 0.12f * shade, 0.95f);
    FrameRect(r, 0.95f, 0.80f, 0.40f, hot ? FRAME_HOT_ALPHA : FRAME_IDLE_ALPHA);
    EndRenderColor();
    PutTextCentered(Bold(), r.x, r.y + BUTTON_TEXT_DY, r.w, label, 255, 244, 214);
}

}
