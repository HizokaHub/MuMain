#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaQueuePanel.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "App/Platform/Windows/Winmain.h"     // g_hFont, g_hFontBold
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaQueueStatus.h"

#include <cmath>

using namespace SEASON3B;
namespace QueueStatus = World::Moba::QueueStatus;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen: upper right corner.
    constexpr int PANEL_W = 226;
    constexpr int PANEL_MARGIN = 6;
    constexpr int PANEL_X = REFERENCE_WIDTH - PANEL_W - PANEL_MARGIN;
    constexpr int PANEL_Y = 8;
    constexpr int PAD = 10;

    constexpr int TITLE_Y = 8;
    constexpr int MODE_Y = 26;
    constexpr int TIME_Y = 44;
    constexpr int STAGES_Y = 66;
    constexpr int STAGES_TITLE_H = 16;
    constexpr int STAGE_ROW_H = 14;
    constexpr int HINT_H = 16;
    constexpr int GAP_BEFORE_BUTTON = 8;
    constexpr int BUTTON_W = 150;
    constexpr int BUTTON_H = 24;
    constexpr int BOTTOM_PAD = 10;
    constexpr int NO_STAGES_H = 18;

    constexpr float LAYER_DEPTH = 9.0f;   // above the game windows, below the message boxes and the ready check popup
    constexpr float KEY_EVENT_ORDER = 0.f;
    constexpr DWORD UI_KEY_OFFSET = 2;
    constexpr int SECONDS_PER_MINUTE = 60;

    const wchar_t* ModeLabel(QueueStatus::Mode mode)
    {
        switch (mode)
        {
        case QueueStatus::Mode::Premade: return L"Equipo de 5 · solo humanos";
        case QueueStatus::Mode::WithBots: return L"5v5 con bots (garantizada)";
        case QueueStatus::Mode::Trio: return L"3v3 · humanos; con bots tras la espera";
        case QueueStatus::Mode::Duel: return L"1v1 · solo humanos";
        case QueueStatus::Mode::Aram: return L"ARAM 5v5 · carril central";
        default: return L"5v5 · solo humanos";
        }
    }

    void FormatClock(wchar_t* out, size_t size, double seconds)
    {
        const int total = static_cast<int>(std::max(0.0, std::floor(seconds)));
        swprintf(out, size, L"%d:%02d", total / SECONDS_PER_MINUTE, total % SECONDS_PER_MINUTE);
    }

    int PanelHeight(const QueueStatus::State& state)
    {
        const int stagesHeight = state.stages.empty()
            ? NO_STAGES_H
            : STAGES_TITLE_H + static_cast<int>(state.stages.size()) * STAGE_ROW_H + HINT_H;
        return STAGES_Y + stagesHeight + GAP_BEFORE_BUTTON + BUTTON_H + BOTTOM_PAD;
    }

    Rect Panel(const QueueStatus::State& state) { return { PANEL_X, PANEL_Y, PANEL_W, PanelHeight(state) }; }

    Rect CancelButton(const QueueStatus::State& state)
    {
        return { PANEL_X + (PANEL_W - BUTTON_W) / 2, PANEL_Y + PanelHeight(state) - BOTTOM_PAD - BUTTON_H, BUTTON_W, BUTTON_H };
    }

    // The index of the step the wait is at (the last one reached); -1 before the first.
    int CurrentStage(const QueueStatus::State& state, double elapsed)
    {
        int current = -1;
        for (size_t i = 0; i < state.stages.size(); ++i)
        {
            if (elapsed >= state.stages[i].seconds)
                current = static_cast<int>(i);
        }
        return current;
    }

    void RenderStageRow(const QueueStatus::State& state, size_t index, int current, int y)
    {
        const QueueStatus::Stage& stage = state.stages[index];
        wchar_t clock[16] = {};
        wchar_t humans[48] = {};
        wchar_t line[96] = {};
        FormatClock(clock, std::size(clock), stage.seconds);
        if (stage.humans <= 1)
            swprintf(humans, std::size(humans), L"cualquier número de humanos");
        else
            swprintf(humans, std::size(humans), L"%d o más humanos", stage.humans);
        swprintf(line, std::size(line), L"%ls   %ls", clock, humans);

        const bool isCurrent = static_cast<int>(index) == current;
        const bool isNext = static_cast<int>(index) == current + 1;
        const BYTE red = isCurrent ? 111 : (isNext ? 241 : 127);
        const BYTE green = isCurrent ? 211 : (isNext ? 196 : 138);
        const BYTE blue = isCurrent ? 154 : (isNext ? 83 : 160);
        Draw::PutTextLeft(isCurrent ? g_hFontBold : g_hFont, PANEL_X + PAD, y, PANEL_W - 2 * PAD, line, red, green, blue);

        if (isCurrent)
        {
            Draw::PutText(g_hFontBold, PANEL_X + PAD, y, PANEL_W - 2 * PAD, L"ahora", 111, 211, 154, RT3_SORT_RIGHT);
        }
        else if (isNext && QueueStatus::NextInSeconds() >= 0.0)
        {
            wchar_t left[24] = {};
            wchar_t text[40] = {};
            FormatClock(left, std::size(left), std::ceil(QueueStatus::NextInSeconds()));
            swprintf(text, std::size(text), L"en %ls", left);
            Draw::PutText(g_hFont, PANEL_X + PAD, y, PANEL_W - 2 * PAD, text, 241, 196, 83, RT3_SORT_RIGHT);
        }
    }

    void RenderStages(const QueueStatus::State& state, double elapsed)
    {
        const int current = CurrentStage(state, elapsed);
        int y = PANEL_Y + STAGES_Y;
        Draw::PutTextLeft(g_hFontBold, PANEL_X + PAD, y, PANEL_W - 2 * PAD, L"Escalones de espera:", 233, 228, 212);
        y += STAGES_TITLE_H;
        for (size_t i = 0; i < state.stages.size(); ++i)
        {
            RenderStageRow(state, i, current, y);
            y += STAGE_ROW_H;
        }
        Draw::PutTextLeft(g_hFont, PANEL_X + PAD, y, PANEL_W - 2 * PAD, L"Antes se intenta completar con humanos.", 127, 138, 160);
    }

    void RenderTexts(const QueueStatus::State& state)
    {
        wchar_t clock[16] = {};
        wchar_t line[96] = {};
        const double elapsed = QueueStatus::ElapsedSeconds();
        Draw::PutTextCentered(g_hFontBold, PANEL_X, PANEL_Y + TITLE_Y, PANEL_W, L"BUSCANDO PARTIDA", 241, 196, 83);
        Draw::PutTextCentered(g_hFont, PANEL_X, PANEL_Y + MODE_Y, PANEL_W, ModeLabel(state.mode), 170, 179, 194);

        FormatClock(clock, std::size(clock), elapsed);
        swprintf(line, std::size(line), L"En cola: %ls  ·  %d jugador(es)", clock, state.waiting);
        Draw::PutTextCentered(g_hFont, PANEL_X, PANEL_Y + TIME_Y, PANEL_W, line, 233, 228, 212);

        if (state.stages.empty())
            Draw::PutTextCentered(g_hFont, PANEL_X, PANEL_Y + STAGES_Y, PANEL_W, L"Esperando jugadores...", 127, 138, 160);
        else
            RenderStages(state, elapsed);
    }
}

void CMobaQueuePanel::Create(CNewUIManager* pNewUIMng)
{
    static CMobaQueuePanel s_panel;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_panel);
}

bool CMobaQueuePanel::Render()
{
    const QueueStatus::State& state = QueueStatus::Get();
    if (state.mode == QueueStatus::Mode::Hidden)
        return true;

    const DWORD backColor = g_pRenderText->GetTextColor();
    Draw::FillRect(Panel(state), 0.04f, 0.055f, 0.086f, 0.88f);
    Draw::FrameRect(Panel(state), 0.78f, 0.64f, 0.29f, 1.f);
    EndRenderColor();
    RenderTexts(state);
    Draw::Button(CancelButton(state), L"CANCELAR BÚSQUEDA", false);
    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaQueuePanel::Update()
{
    return true;
}

bool CMobaQueuePanel::UpdateMouseEvent()
{
    const QueueStatus::State& state = QueueStatus::Get();
    if (state.mode == QueueStatus::Mode::Hidden || !Draw::MouseInRect(Panel(state)))
        return true;

    if (IsPress(VK_LBUTTON) && Draw::MouseInRect(CancelButton(state)))
        QueueStatus::Cancel();

    // The panel swallows the mouse: a click on it must not walk the hero.
    return false;
}

bool CMobaQueuePanel::UpdateKeyEvent()
{
    return true;
}

float CMobaQueuePanel::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaQueuePanel::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
