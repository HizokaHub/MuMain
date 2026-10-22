#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaReadyCheckWindow.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "App/Platform/Windows/Winmain.h"     // Draw::Body(), Draw::Title()
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaReadyCheck.h"

#include <algorithm>
#include <cmath>

using namespace SEASON3B;
namespace ReadyCheck = World::Moba::ReadyCheck;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

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
    constexpr int HINT_Y = 160;
    constexpr int WAITING_TEXT_Y = 134;

    constexpr float LAYER_DEPTH = 10.8f;       // above the message boxes (10.7): it is drawn last and gets the mouse first
    constexpr float KEY_EVENT_ORDER = 100.0f;  // before the chat box and the windows, so Enter accepts instead of opening the chat
    constexpr DWORD UI_KEY_OFFSET = 1;

    constexpr float MS_PER_SECOND = 1000.0f;
    constexpr int MAX_SLOTS = 10;
    constexpr float LATE_SHARE = 0.34f;

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

    void RenderCountdownBar(const ReadyCheck::State& state)
    {
        const double window = std::max(state.windowMs, 1.0);
        const float share = static_cast<float>(std::clamp(ReadyCheck::RemainingMs() / window, 0.0, 1.0));
        const Rect bar = { PANEL_X + BAR_MARGIN, PANEL_Y + BAR_Y, PANEL_W - 2 * BAR_MARGIN, BAR_H };
        Draw::FillRect(bar, 0.13f, 0.16f, 0.22f, 0.95f);
        // Gold while there is time, turning red in the last third.
        const bool late = share < LATE_SHARE;
        Draw::FillRect({ bar.x, bar.y, static_cast<int>(bar.w * share), bar.h }, late ? 0.85f : 0.78f, late ? 0.25f : 0.64f, late ? 0.20f : 0.29f, 1.f);
        Draw::FrameRect(bar, 0.23f, 0.27f, 0.35f, 1.f);
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
                Draw::FillRect(slot, 0.18f, 0.55f, 0.34f, 1.f);
            else
                Draw::FillRect(slot, 0.09f, 0.11f, 0.16f, 1.f);
            Draw::FrameRect(slot, accepted ? 0.44f : 0.23f, accepted ? 0.83f : 0.27f, accepted ? 0.60f : 0.35f, 1.f);
        }
    }

    void RenderTexts(const ReadyCheck::State& state)
    {
        using Draw::PutTextCentered;
        wchar_t line[128] = {};
        PutTextCentered(Draw::Title(), PANEL_X, PANEL_Y + TITLE_Y, PANEL_W, L"PARTIDA ENCONTRADA", 241, 196, 83);

        const int seconds = static_cast<int>(std::ceil(ReadyCheck::RemainingMs() / MS_PER_SECOND));
        swprintf(line, std::size(line), L"Acepta en %d s o saldr\u00e1s de la cola", seconds);
        PutTextCentered(Draw::Body(), PANEL_X, PANEL_Y + SUBTITLE_Y, PANEL_W, line, 233, 228, 212);

        swprintf(line, std::size(line), L"%d/%d jugadores han aceptado", state.accepted, state.total);
        PutTextCentered(Draw::Body(), PANEL_X, PANEL_Y + COUNT_TEXT_Y, PANEL_W, line, 170, 179, 194);

        if (state.status == ReadyCheck::Status::Accepted)
            PutTextCentered(Draw::Bold(), PANEL_X, PANEL_Y + WAITING_TEXT_Y, PANEL_W, L"Aceptaste. Esperando a los dem\u00e1s...", 111, 211, 154);
        else
            PutTextCentered(Draw::Body(), PANEL_X, PANEL_Y + HINT_Y, PANEL_W, L"Intro = aceptar", 127, 138, 160);
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
    Draw::FillRect(Panel(), 0.04f, 0.055f, 0.086f, 0.92f);
    Draw::FrameRect(Panel(), 0.78f, 0.64f, 0.29f, 1.f);
    RenderCountdownBar(state);
    RenderPlayerSlots(state);
    EndRenderColor();
    if (state.status == ReadyCheck::Status::Asking)
    {
        Draw::Button(AcceptButton(), L"ACEPTAR", true);
        Draw::Button(DeclineButton(), L"RECHAZAR", false);
    }
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
    if (Draw::HeldClickSwallowed())
        return false;

    const ReadyCheck::State& state = ReadyCheck::Get();
    if (state.status == ReadyCheck::Status::Hidden || !Draw::MouseInRect(Panel()))
        return true;

    if (state.status == ReadyCheck::Status::Asking && IsPress(VK_LBUTTON))
    {
        Draw::SwallowHeldClick();
        if (Draw::MouseInRect(AcceptButton()))
            ReadyCheck::Answer(true);
        else if (Draw::MouseInRect(DeclineButton()))
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
