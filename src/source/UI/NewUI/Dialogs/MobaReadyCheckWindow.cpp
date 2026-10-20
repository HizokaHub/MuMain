#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaReadyCheckWindow.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress, CheckMouseIn
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "App/Platform/Windows/Winmain.h"     // g_hFont, g_hFontBig
#include "Render/Textures/ZzzOpenglUtil.h"    // RenderColor, EndRenderColor
#include "World/Moba/MobaReadyCheck.h"

#include <algorithm>
#include <cmath>

using namespace SEASON3B;
namespace ReadyCheck = World::Moba::ReadyCheck;

namespace
{
    // Layout, in the 640x480 reference screen.
    constexpr int PANEL_W = 340;
    constexpr int PANEL_H = 176;
    constexpr int PANEL_X = (REFERENCE_WIDTH - PANEL_W) / 2;
    constexpr int PANEL_Y = 70;

    constexpr int TITLE_Y = 12;
    constexpr int SUBTITLE_Y = 46;
    constexpr int BAR_Y = 68;
    constexpr int BAR_H = 8;
    constexpr int BAR_MARGIN = 32;
    constexpr int SLOTS_Y = 86;
    constexpr int SLOT_SIZE = 18;
    constexpr int SLOT_GAP = 6;
    constexpr int COUNT_TEXT_Y = 108;
    constexpr int BUTTONS_Y = 128;
    constexpr int BUTTON_W = 130;
    constexpr int BUTTON_H = 26;
    constexpr int BUTTON_GAP = 20;
    constexpr int BUTTON_TEXT_DY = 7;
    constexpr int HINT_Y = 160;
    constexpr int WAITING_TEXT_Y = 134;
    constexpr int BORDER = 1;

    constexpr float LAYER_DEPTH = 10.8f;       // above the message boxes (10.7): it is drawn last and gets the mouse first
    constexpr float KEY_EVENT_ORDER = 100.0f;  // before the chat box and the windows, so Enter accepts instead of opening the chat
    constexpr DWORD UI_KEY_OFFSET = 1;

    constexpr float MS_PER_SECOND = 1000.0f;
    constexpr int ALPHA_FULL = 255;
    constexpr int MAX_SLOTS = 10;

    struct Rect
    {
        int x, y, w, h;
    };

    Rect Panel() { return { PANEL_X, PANEL_Y, PANEL_W, PANEL_H }; }

    Rect AcceptButton()
    {
        const int left = PANEL_X + (PANEL_W - (2 * BUTTON_W + BUTTON_GAP)) / 2;
        return { left, PANEL_Y + BUTTONS_Y, BUTTON_W, BUTTON_H };
    }

    Rect DeclineButton()
    {
        const Rect accept = AcceptButton();
        return { accept.x + BUTTON_W + BUTTON_GAP, accept.y, BUTTON_W, BUTTON_H };
    }

    bool MouseInRect(const Rect& r) { return SEASON3B::CheckMouseIn(r.x, r.y, r.w, r.h); }

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

    void PutText(HFONT font, int x, int y, int width, const wchar_t* text, BYTE red, BYTE green, BYTE blue)
    {
        g_pRenderText->SetFont(font);
        g_pRenderText->SetBgColor(0, 0, 0, 0);
        g_pRenderText->SetTextColor(red, green, blue, ALPHA_FULL);
        g_pRenderText->RenderText(x, y, text, width, 0, RT3_SORT_CENTER);
    }

    void RenderButton(const Rect& r, const wchar_t* label, bool positive)
    {
        const bool hot = MouseInRect(r);
        const float base = hot ? 1.f : 0.75f;
        if (positive)
            FillRect(r, 0.12f * base, 0.40f * base, 0.25f * base, 0.95f);
        else
            FillRect(r, 0.30f * base, 0.12f * base, 0.12f * base, 0.95f);
        FrameRect(r, 0.95f, 0.80f, 0.40f, hot ? 1.f : 0.6f);
        EndRenderColor();
        PutText(g_hFontBold, r.x, r.y + BUTTON_TEXT_DY, r.w, label, 255, 244, 214);
    }

    void RenderCountdownBar(const ReadyCheck::State& state)
    {
        const double window = std::max(state.windowMs, 1.0);
        const float share = static_cast<float>(std::clamp(ReadyCheck::RemainingMs() / window, 0.0, 1.0));
        const Rect bar = { PANEL_X + BAR_MARGIN, PANEL_Y + BAR_Y, PANEL_W - 2 * BAR_MARGIN, BAR_H };
        FillRect(bar, 0.13f, 0.16f, 0.22f, 0.95f);
        // Gold while there is time, turning red in the last third.
        const bool late = share < 0.34f;
        FillRect({ bar.x, bar.y, static_cast<int>(bar.w * share), bar.h }, late ? 0.85f : 0.78f, late ? 0.25f : 0.64f, late ? 0.20f : 0.29f, 1.f);
        FrameRect(bar, 0.23f, 0.27f, 0.35f, 1.f);
    }

    void RenderPlayerSlots(const ReadyCheck::State& state)
    {
        const int count = std::clamp(state.total, 0, MAX_SLOTS);
        const int width = count * SLOT_SIZE + (count - 1) * SLOT_GAP;
        const int left = PANEL_X + (PANEL_W - width) / 2;
        for (int i = 0; i < count; ++i)
        {
            const Rect slot = { left + i * (SLOT_SIZE + SLOT_GAP), PANEL_Y + SLOTS_Y, SLOT_SIZE, SLOT_SIZE };
            const bool accepted = i < state.accepted;
            if (accepted)
                FillRect(slot, 0.18f, 0.55f, 0.34f, 1.f);
            else
                FillRect(slot, 0.09f, 0.11f, 0.16f, 1.f);
            FrameRect(slot, accepted ? 0.44f : 0.23f, accepted ? 0.83f : 0.27f, accepted ? 0.60f : 0.35f, 1.f);
        }
    }

    void RenderTexts(const ReadyCheck::State& state)
    {
        wchar_t line[128] = {};
        PutText(g_hFontBig, PANEL_X, PANEL_Y + TITLE_Y, PANEL_W, L"PARTIDA ENCONTRADA", 241, 196, 83);

        const int seconds = static_cast<int>(std::ceil(ReadyCheck::RemainingMs() / MS_PER_SECOND));
        swprintf(line, std::size(line), L"Acepta en %d s o saldrás de la cola", seconds);
        PutText(g_hFont, PANEL_X, PANEL_Y + SUBTITLE_Y, PANEL_W, line, 233, 228, 212);

        swprintf(line, std::size(line), L"%d/%d jugadores han aceptado", state.accepted, state.total);
        PutText(g_hFont, PANEL_X, PANEL_Y + COUNT_TEXT_Y, PANEL_W, line, 170, 179, 194);

        if (state.status == ReadyCheck::Status::Accepted)
            PutText(g_hFontBold, PANEL_X, PANEL_Y + WAITING_TEXT_Y, PANEL_W, L"Aceptaste. Esperando a los demás...", 111, 211, 154);
        else
            PutText(g_hFont, PANEL_X, PANEL_Y + HINT_Y, PANEL_W, L"Intro = aceptar", 127, 138, 160);
    }
}

void CMobaReadyCheckWindow::Create(CNewUIManager* pNewUIMng)
{
    static CMobaReadyCheckWindow s_window;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_window);
}

bool CMobaReadyCheckWindow::Render()
{
    const ReadyCheck::State& state = ReadyCheck::Get();
    if (state.status == ReadyCheck::Status::Hidden)
        return true;

    const DWORD backColor = g_pRenderText->GetTextColor();
    const Rect panel = Panel();
    FillRect(panel, 0.04f, 0.055f, 0.086f, 0.92f);
    FrameRect(panel, 0.78f, 0.64f, 0.29f, 1.f);
    RenderCountdownBar(state);
    RenderPlayerSlots(state);
    if (state.status == ReadyCheck::Status::Asking)
    {
        RenderButton(AcceptButton(), L"ACEPTAR", true);
        RenderButton(DeclineButton(), L"RECHAZAR", false);
    }
    EndRenderColor();
    RenderTexts(state);
    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaReadyCheckWindow::Update()
{
    return true;
}

bool CMobaReadyCheckWindow::UpdateMouseEvent()
{
    const ReadyCheck::State& state = ReadyCheck::Get();
    if (state.status == ReadyCheck::Status::Hidden || !MouseInRect(Panel()))
        return true;

    if (state.status == ReadyCheck::Status::Asking && IsPress(VK_LBUTTON))
    {
        if (MouseInRect(AcceptButton()))
            ReadyCheck::Answer(true);
        else if (MouseInRect(DeclineButton()))
            ReadyCheck::Answer(false);
    }

    // The panel swallows the mouse: a click on it must not walk the hero.
    return false;
}

bool CMobaReadyCheckWindow::UpdateKeyEvent()
{
    if (ReadyCheck::Get().status != ReadyCheck::Status::Asking || !IsPress(VK_RETURN))
        return true;

    ReadyCheck::Answer(true);
    return false;
}

float CMobaReadyCheckWindow::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaReadyCheckWindow::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
