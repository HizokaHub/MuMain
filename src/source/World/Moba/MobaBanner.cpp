#include "stdafx.h"
#include "World/Moba/MobaBanner.h"

#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "App/Platform/Windows/Winmain.h"     // g_hFontBig
#include "Data/Translation/MultiLanguage.h"   // CMultiLanguage::ConvertFromUtf8

#include <algorithm>

extern double WorldTime;

namespace World::Moba::Banner
{

namespace
{
    constexpr int BANNER_HEADER_BYTES = 6;
    constexpr int BANNER_TEXT_CHARS = 96;
    constexpr double BANNER_MIN_MS = 500.0;
    constexpr double BANNER_MAX_MS = 10000.0;

    // Fade in and out, as a share of the duration; the text sits in the upper middle of the 640x480 reference screen.
    constexpr double BANNER_FADE_MS = 400.0;
    constexpr int BANNER_TEXT_Y = 110;

    // Gold, with a dark shadow so it reads over any floor.
    constexpr int GOLD_RED = 255;
    constexpr int GOLD_GREEN = 214;
    constexpr int GOLD_BLUE = 92;
    constexpr int SHADOW_OFFSET = 2;
    constexpr int SHADOW_ALPHA_MAX = 220;
    constexpr int ALPHA_MAX = 255;

    wchar_t s_text[BANNER_TEXT_CHARS] = {};
    double s_startMs = 0.0;
    double s_durationMs = 0.0;

    int AlphaAt(double elapsedMs)
    {
        const double remaining = s_durationMs - elapsedMs;
        const double edge = std::min(elapsedMs, remaining);
        return static_cast<int>(ALPHA_MAX * std::clamp(edge / BANNER_FADE_MS, 0.0, 1.0));
    }
}

void Receive(const unsigned char* ReceiveBuffer)
{
    const int length = ReceiveBuffer[1];
    if (length <= BANNER_HEADER_BYTES)
        return;

    const double durationMs = ReceiveBuffer[4] | (ReceiveBuffer[5] << 8);
    char utf8[BANNER_TEXT_CHARS * 4] = {};
    const int textBytes = std::min(length - BANNER_HEADER_BYTES, static_cast<int>(sizeof(utf8)) - 1);
    memcpy(utf8, ReceiveBuffer + BANNER_HEADER_BYTES, textBytes);

    memset(s_text, 0, sizeof(s_text));
    CMultiLanguage::ConvertFromUtf8(s_text, utf8, textBytes);
    s_startMs = WorldTime;
    s_durationMs = std::clamp(durationMs, BANNER_MIN_MS, BANNER_MAX_MS);
}

void Show(const wchar_t* text, double durationMs)
{
    memset(s_text, 0, sizeof(s_text));
    wcsncpy_s(s_text, std::size(s_text), text, _TRUNCATE);
    s_startMs = WorldTime;
    s_durationMs = std::clamp(durationMs, BANNER_MIN_MS, BANNER_MAX_MS);
}

void Render()
{
    if (s_durationMs <= 0.0)
        return;

    const double elapsed = WorldTime - s_startMs;
    if (elapsed >= s_durationMs)
    {
        s_durationMs = 0.0;
        return;
    }

    const int alpha = AlphaAt(elapsed);
    if (alpha <= 0)
        return;

    DWORD backColor = g_pRenderText->GetTextColor();
    DWORD backBg = g_pRenderText->GetBgColor();
    g_pRenderText->SetFont(g_hFontBig);
    g_pRenderText->SetBgColor(0, 0, 0, 0);

    g_pRenderText->SetTextColor(0, 0, 0, alpha * SHADOW_ALPHA_MAX / ALPHA_MAX);
    g_pRenderText->RenderText(SHADOW_OFFSET, BANNER_TEXT_Y + SHADOW_OFFSET, s_text, REFERENCE_WIDTH, 0, RT3_SORT_CENTER);
    g_pRenderText->SetTextColor(GOLD_RED, GOLD_GREEN, GOLD_BLUE, alpha);
    g_pRenderText->RenderText(0, BANNER_TEXT_Y, s_text, REFERENCE_WIDTH, 0, RT3_SORT_CENTER);

    g_pRenderText->SetTextColor(backColor);
    g_pRenderText->SetBgColor(backBg);
}

}
