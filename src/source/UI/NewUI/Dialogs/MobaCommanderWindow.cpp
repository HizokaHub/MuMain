#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaCommanderWindow.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaCommander.h"

#include <algorithm>

using namespace SEASON3B;
namespace Commander = World::Moba::Commander;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen.
    constexpr int PANEL_X = 4;
    constexpr int PANEL_Y = 100;
    constexpr int PANEL_W = 178;
    constexpr int MARGIN = 8;
    constexpr int CONTENT_W = PANEL_W - 2 * MARGIN;

    constexpr int TITLE_Y = 6;
    constexpr int ESSENCE_Y = 22;
    constexpr int INCOME_TEXT_Y = 48;
    constexpr int FIRST_BUTTON_Y = 68;
    constexpr int BUTTON_H = 20;
    constexpr int UNIT_ROW_H = 42;
    constexpr int UNIT_LABEL_DY = 2;
    constexpr int LANE_BUTTON_DY = 19;
    constexpr int LANE_BUTTON_H = 19;
    constexpr int LANE_GAP = 4;
    constexpr int SMALL_GAP = 4;
    constexpr int BOTTOM_PADDING = 8;

    constexpr float LAYER_DEPTH = 10.6f;       // below the popups (ready check 10.8) but above the HUD
    constexpr float KEY_EVENT_ORDER = 0.0f;
    constexpr DWORD UI_KEY_OFFSET = 15;

    constexpr float HOT_SHADE = 1.35f;
    constexpr float IDLE_SHADE = 1.0f;

    const wchar_t* const UNIT_NAMES[] = { L"Soldados", L"Brutos", L"Élite" };
    const wchar_t* const LANE_NAMES[] = { L"TOP", L"MID", L"BOT" };

    int UnitsHeight(const Commander::State& state)
    {
        return state.unitCount * UNIT_ROW_H;
    }

    // The panel grows with what it shows: the commander's controls, a single button, or only the numbers.
    int PanelHeight(const Commander::State& state)
    {
        if (state.isCommander)
            return FIRST_BUTTON_Y + BUTTON_H + SMALL_GAP + UnitsHeight(state) + BUTTON_H + BOTTOM_PADDING;
        if (state.commandFree)
            return FIRST_BUTTON_Y + BUTTON_H + BOTTOM_PADDING;
        return FIRST_BUTTON_Y + BOTTOM_PADDING;
    }

    Rect Panel(const Commander::State& state) { return { PANEL_X, PANEL_Y, PANEL_W, PanelHeight(state) }; }

    Rect FullWidthButton(int y) { return { PANEL_X + MARGIN, PANEL_Y + y, CONTENT_W, BUTTON_H }; }

    Rect IncomeButton() { return FullWidthButton(FIRST_BUTTON_Y); }

    Rect ClaimButton() { return FullWidthButton(FIRST_BUTTON_Y); }

    int UnitsTop() { return FIRST_BUTTON_Y + BUTTON_H + SMALL_GAP; }

    Rect ReleaseButton(const Commander::State& state) { return FullWidthButton(UnitsTop() + UnitsHeight(state)); }

    int UnitRowY(int unit) { return PANEL_Y + UnitsTop() + unit * UNIT_ROW_H; }

    Rect LaneButton(int unit, int lane)
    {
        const int width = (CONTENT_W - (Commander::LANE_COUNT - 1) * LANE_GAP) / Commander::LANE_COUNT;
        return { PANEL_X + MARGIN + lane * (width + LANE_GAP), UnitRowY(unit) + LANE_BUTTON_DY, width, LANE_BUTTON_H };
    }

    bool CanSend(const Commander::State& state, int unit, int lane)
    {
        return Commander::IsLaneOpen(state, lane) && state.essence >= state.unitCost[unit];
    }

    bool CanBuyIncome(const Commander::State& state)
    {
        return state.nextIncomeCost > 0 && state.essence >= state.nextIncomeCost;
    }

    const wchar_t* UnitName(int unit)
    {
        return unit < static_cast<int>(std::size(UNIT_NAMES)) ? UNIT_NAMES[unit] : L"Paquete";
    }

    // A button; it is grayed and does nothing while it is not enabled.
    void DrawButton(const Rect& r, const wchar_t* label, bool enabled)
    {
        const bool hot = enabled && Draw::MouseInRect(r);
        const float shade = hot ? HOT_SHADE : IDLE_SHADE;
        if (enabled)
            Draw::FillRect(r, 0.12f * shade, 0.36f * shade, 0.24f * shade, 0.95f);
        else
            Draw::FillRect(r, 0.09f, 0.11f, 0.16f, 0.95f);
        Draw::FrameRect(r, enabled ? 0.78f : 0.23f, enabled ? 0.64f : 0.27f, enabled ? 0.29f : 0.35f, 1.f);
        EndRenderColor();
        const int textY = r.y + (r.h - Draw::BodyLineHeight()) / 2;
        if (enabled)
            Draw::PutTextCentered(Draw::Body(), r.x, textY, r.w, label, 255, 244, 214);
        else
            Draw::PutTextCentered(Draw::Body(), r.x, textY, r.w, label, 110, 118, 135);
    }

    void RenderHeader(const Commander::State& state)
    {
        wchar_t line[96] = {};
        Draw::PutTextCentered(Draw::Bold(), PANEL_X, PANEL_Y + TITLE_Y, PANEL_W, state.isCommander ? L"COMANDANTE" : L"ESENCIA DEL EQUIPO", 241, 196, 83);
        swprintf(line, std::size(line), L"%d", state.essence);
        Draw::PutTextCentered(Draw::Title(), PANEL_X, PANEL_Y + ESSENCE_Y, PANEL_W, line, 233, 228, 212);
        swprintf(line, std::size(line), L"+%d/s  ·  ingreso %d/%d", state.incomePerSecond, state.incomeLevel, state.maxIncomeLevel);
        Draw::PutTextCentered(Draw::Body(), PANEL_X, PANEL_Y + INCOME_TEXT_Y, PANEL_W, line, 170, 179, 194);
    }

    void RenderIncomeButton(const Commander::State& state)
    {
        wchar_t label[64] = {};
        if (state.nextIncomeCost > 0)
            swprintf(label, std::size(label), L"Mejorar ingreso (%d)", state.nextIncomeCost);
        else
            swprintf(label, std::size(label), L"Ingreso al máximo");
        DrawButton(IncomeButton(), label, CanBuyIncome(state));
    }

    void RenderUnitRow(const Commander::State& state, int unit)
    {
        wchar_t label[64] = {};
        swprintf(label, std::size(label), L"%ls  ·  %d", UnitName(unit), state.unitCost[unit]);
        Draw::PutTextLeft(Draw::Body(), PANEL_X + MARGIN, UnitRowY(unit) + UNIT_LABEL_DY, CONTENT_W, label, 233, 228, 212);
        for (int lane = 0; lane < Commander::LANE_COUNT; ++lane)
            DrawButton(LaneButton(unit, lane), LANE_NAMES[lane], CanSend(state, unit, lane));
    }

    void RenderCommanderControls(const Commander::State& state)
    {
        RenderIncomeButton(state);
        for (int unit = 0; unit < state.unitCount; ++unit)
            RenderUnitRow(state, unit);
        DrawButton(ReleaseButton(state), L"Soltar el mando", true);
    }

    void HandleCommanderClick(const Commander::State& state)
    {
        if (Draw::MouseInRect(IncomeButton()) && CanBuyIncome(state))
        {
            Commander::Order(Commander::Action::Income);
            return;
        }

        if (Draw::MouseInRect(ReleaseButton(state)))
        {
            Commander::Order(Commander::Action::Release);
            return;
        }

        for (int unit = 0; unit < state.unitCount; ++unit)
        {
            for (int lane = 0; lane < Commander::LANE_COUNT; ++lane)
            {
                if (Draw::MouseInRect(LaneButton(unit, lane)) && CanSend(state, unit, lane))
                    Commander::Order(Commander::Action::Send, unit, lane);
            }
        }
    }
}

void CMobaCommanderWindow::Create(CNewUIManager* pNewUIMng)
{
    static CMobaCommanderWindow s_window;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_window);
}

bool CMobaCommanderWindow::Render()
{
    const Commander::State& state = Commander::Get();
    if (!state.visible)
        return true;

    const DWORD backColor = g_pRenderText->GetTextColor();
    Draw::FillRect(Panel(state), 0.04f, 0.055f, 0.086f, 0.88f);
    Draw::FrameRect(Panel(state), 0.78f, 0.64f, 0.29f, 1.f);
    EndRenderColor();
    RenderHeader(state);
    if (state.isCommander)
        RenderCommanderControls(state);
    else if (state.commandFree)
        DrawButton(ClaimButton(), L"Tomar el mando", true);
    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaCommanderWindow::Update()
{
    return true;
}

bool CMobaCommanderWindow::UpdateMouseEvent()
{
    if (Draw::HeldClickSwallowed())
        return false;

    const Commander::State& state = Commander::Get();
    if (!state.visible || !Draw::MouseInRect(Panel(state)))
        return true;

    if (IsPress(VK_LBUTTON))
    {
        Draw::SwallowHeldClick();
        if (state.isCommander)
            HandleCommanderClick(state);
        else if (state.commandFree && Draw::MouseInRect(ClaimButton()))
            Commander::Order(Commander::Action::Claim);
    }

    // The panel swallows the mouse: a click on it must not walk the hero.
    return false;
}

bool CMobaCommanderWindow::UpdateKeyEvent()
{
    return true;
}

float CMobaCommanderWindow::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaCommanderWindow::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
