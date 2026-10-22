#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaLoadoutWindow.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "App/Platform/Windows/Winmain.h"     // g_hFont, g_hFontBold, g_hFontBig
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "Engine/Object/ZzzInfomation.h"      // SkillAttribute
#include "UI/NewUI/HUD/Skills/SkillIcon.h"
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaLoadout.h"

#include <algorithm>

using namespace SEASON3B;
namespace Loadout = World::Moba::Loadout;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen.
    constexpr int PANEL_W = 392;
    constexpr int PANEL_H = 300;
    constexpr int PANEL_X = (REFERENCE_WIDTH - PANEL_W) / 2;
    constexpr int PANEL_Y = 70;

    constexpr int TITLE_Y = 10;
    constexpr int SUBTITLE_Y = 38;
    constexpr int GRID_X = 28;
    constexpr int GRID_Y = 64;
    constexpr int GRID_COLUMNS = 8;
    constexpr int CELL_W = 42;
    constexpr int CELL_H = 46;
    constexpr int ICON_W = 20;  // the size of an icon in the skill atlas: the texture coordinates depend on it
    constexpr int ICON_H = 28;
    constexpr int NAME_GAP = 8;
    constexpr int BUTTON_W = 130;
    constexpr int BUTTON_H = 26;
    constexpr int BUTTON_GAP = 20;
    constexpr int BUTTON_BOTTOM_MARGIN = 16;
    constexpr int TEXT_DY = 2;

    constexpr float LAYER_DEPTH = 10.85f;      // above the ready-check popup (10.8)
    constexpr float KEY_EVENT_ORDER = 100.0f;
    constexpr DWORD UI_KEY_OFFSET = 4;

    Rect Panel() { return { PANEL_X, PANEL_Y, PANEL_W, PANEL_H }; }

    Rect CellOf(int index)
    {
        return { PANEL_X + GRID_X + (index % GRID_COLUMNS) * CELL_W, PANEL_Y + GRID_Y + (index / GRID_COLUMNS) * CELL_H, CELL_W - 2, CELL_H - 2 };
    }

    Rect SaveButton()
    {
        const int left = PANEL_X + (PANEL_W - (2 * BUTTON_W + BUTTON_GAP)) / 2;
        return { left, PANEL_Y + PANEL_H - BUTTON_BOTTOM_MARGIN - BUTTON_H, BUTTON_W, BUTTON_H };
    }

    Rect CancelButton()
    {
        const Rect save = SaveButton();
        return { save.x + BUTTON_W + BUTTON_GAP, save.y, BUTTON_W, BUTTON_H };
    }

    // The index of the cell under the mouse, or -1.
    int HoveredCell(const Loadout::State& state)
    {
        for (int i = 0; i < static_cast<int>(state.selectable.size()); ++i)
        {
            if (Draw::MouseInRect(CellOf(i)))
                return i;
        }

        return -1;
    }

    void RenderCell(int index, unsigned short skill, bool hovered)
    {
        const Rect cell = CellOf(index);
        const bool picked = Loadout::IsSelected(skill);
        Draw::FillRect(cell, picked ? 0.14f : 0.07f, picked ? 0.20f : 0.09f, picked ? 0.12f : 0.13f, 0.95f);
        if (picked)
            Draw::FrameRect(cell, 0.95f, 0.80f, 0.40f, 1.f);
        else
            Draw::FrameRect(cell, 0.23f, 0.27f, 0.35f, hovered ? 1.f : 0.7f);
        EndRenderColor();

        UI::Skills::Icon::Render(skill, static_cast<float>(cell.x + (cell.w - ICON_W) / 2), static_cast<float>(cell.y + (cell.h - ICON_H) / 2), ICON_W, ICON_H);
        if (!picked)
        {
            Draw::FillRect(cell, 0.f, 0.f, 0.f, 0.45f);
            EndRenderColor();
        }
    }

    void RenderSaveButton()
    {
        if (Loadout::IsComplete())
        {
            Draw::Button(SaveButton(), L"GUARDAR", true);
            return;
        }

        const Rect save = SaveButton();
        Draw::FillRect(save, 0.15f, 0.16f, 0.19f, 0.95f);
        Draw::FrameRect(save, 0.40f, 0.40f, 0.40f, 0.6f);
        EndRenderColor();
        Draw::PutTextCentered(g_hFontBold, save.x, save.y + TEXT_DY + 5, save.w, L"GUARDAR", 110, 110, 110);
    }

    void RenderTexts(const Loadout::State& state, int hovered)
    {
        Draw::PutTextCentered(g_hFontBig, PANEL_X, PANEL_Y + TITLE_Y, PANEL_W, L"ELEGIR MIS SKILLS", 241, 196, 83);

        wchar_t line[128] = {};
        const bool complete = Loadout::IsComplete();
        swprintf(line, std::size(line), L"Elige entre %d y %d skills para la partida: %d/%d", state.minSkills, state.maxSkills, static_cast<int>(state.selected.size()), state.maxSkills);
        Draw::PutTextCentered(g_hFont, PANEL_X, PANEL_Y + SUBTITLE_Y, PANEL_W, line, complete ? 111 : 233, complete ? 211 : 228, complete ? 154 : 212);

        const int rows = (static_cast<int>(state.selectable.size()) + GRID_COLUMNS - 1) / GRID_COLUMNS;
        const int nameY = PANEL_Y + GRID_Y + rows * CELL_H + NAME_GAP;
        if (hovered >= 0)
            Draw::PutTextCentered(g_hFontBold, PANEL_X, nameY + TEXT_DY, PANEL_W, SkillAttribute[state.selectable[hovered]].Name, 233, 228, 212);
        else
            Draw::PutTextCentered(g_hFont, PANEL_X, nameY + TEXT_DY, PANEL_W, L"Pasa el mouse sobre una skill para ver su nombre", 127, 138, 160);
    }
}

void CMobaLoadoutWindow::Create(CNewUIManager* pNewUIMng)
{
    static CMobaLoadoutWindow s_window;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_window);
}

bool CMobaLoadoutWindow::Render()
{
    const Loadout::State& state = Loadout::Get();
    if (!state.open)
        return true;

    const DWORD backColor = g_pRenderText->GetTextColor();
    const int hovered = HoveredCell(state);
    Draw::FillRect(Panel(), 0.04f, 0.055f, 0.086f, 0.94f);
    Draw::FrameRect(Panel(), 0.78f, 0.64f, 0.29f, 1.f);
    EndRenderColor();

    for (int i = 0; i < static_cast<int>(state.selectable.size()); ++i)
        RenderCell(i, state.selectable[i], i == hovered);

    RenderSaveButton();
    Draw::Button(CancelButton(), L"CANCELAR", false);
    RenderTexts(state, hovered);
    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaLoadoutWindow::Update()
{
    return true;
}

bool CMobaLoadoutWindow::UpdateMouseEvent()
{
    const Loadout::State& state = Loadout::Get();
    if (!state.open || !Draw::MouseInRect(Panel()))
        return true;

    if (IsPress(VK_LBUTTON))
    {
        const int cell = HoveredCell(state);
        if (cell >= 0)
            Loadout::Toggle(state.selectable[cell]);
        else if (Draw::MouseInRect(SaveButton()))
            Loadout::Save();
        else if (Draw::MouseInRect(CancelButton()))
            Loadout::Close();
    }

    // The window swallows the mouse: a click on it must not walk the hero.
    return false;
}

bool CMobaLoadoutWindow::UpdateKeyEvent()
{
    if (!Loadout::Get().open || !IsPress(VK_ESCAPE))
        return true;

    Loadout::Close();
    return false;
}

float CMobaLoadoutWindow::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaLoadoutWindow::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
