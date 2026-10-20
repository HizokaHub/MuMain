#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"

#include "UI/NewUI/NewUICommon.h"             // CheckMouseIn
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "App/Platform/Windows/Winmain.h"     // g_hFontBold
#include "Render/Textures/ZzzOpenglUtil.h"    // RenderColor, EndRenderColor

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
    PutTextCentered(g_hFontBold, r.x, r.y + BUTTON_TEXT_DY, r.w, label, 255, 244, 214);
}

}
