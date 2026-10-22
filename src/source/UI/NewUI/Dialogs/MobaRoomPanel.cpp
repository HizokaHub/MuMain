#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaRoomPanel.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/NewUI/HUD/NewUIMiniMap.h"        // the portrait atlas index
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor, RenderBitmap
#include "Render/Textures/ZzzTexture.h"       // LoadBitmap
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaRoom.h"

#include <algorithm>

using namespace SEASON3B;
namespace Room = World::Moba::Room;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen.
    constexpr int PANEL_W = 460;
    constexpr int PAD = 16;
    constexpr int PANEL_Y_TOP = 40;

    constexpr int CODE_LABEL_Y = 8;
    constexpr int CODE_Y = 22;
    constexpr int SUBTITLE_Y = 56;
    constexpr int TEAMS_Y = 82;
    constexpr int TEAM_HEADER_H = 20;
    constexpr int SLOT_H = 38;
    constexpr int SLOT_GAP = 4;
    constexpr int PORTRAIT_SIZE = 32;
    constexpr int NAME_X = 42;
    constexpr int NAME_DY = 9;
    constexpr int BUTTON_W = 130;
    constexpr int BUTTON_H = 26;
    constexpr int BUTTON_GAP = 10;
    constexpr int BUTTONS_TOP_GAP = 10;
    constexpr int BOTTOM_PAD = 14;
    constexpr int TEXT_DY = 3;

    constexpr int TAB_W = 130;
    constexpr int TAB_H = 24;
    constexpr int TAB_X = 6;
    constexpr int TAB_Y = 44;

    constexpr int ATLAS_COLS = 8;
    constexpr int ATLAS_ROWS = 2;
    constexpr int PORTRAIT_CLASS_COUNT = 7;

    constexpr float LAYER_DEPTH = 10.6f;
    constexpr float KEY_EVENT_ORDER = 50.0f;
    constexpr DWORD UI_KEY_OFFSET = 5;

    // The buttons in the order they are laid out; the bots and start buttons are the host's.
    struct ButtonSpec
    {
        Room::Action action;
        bool hostOnly;
    };

    constexpr ButtonSpec BUTTONS[] = {
        { Room::Action::SwitchTeam, false },
        { Room::Action::ToggleBots, true },
        { Room::Action::Start, true },
        { Room::Action::Leave, false },
    };

    bool s_collapsed = false;
    unsigned int s_portraitsLoadedFor = 0;

    int Slots(const Room::State& state)
    {
        return std::max(state.teamSize, 1);
    }

    int ButtonCount(const Room::State& state)
    {
        return static_cast<int>(std::count_if(std::begin(BUTTONS), std::end(BUTTONS), [&](const ButtonSpec& b) { return !b.hostOnly || state.isHost; }));
    }

    int PanelHeight(const Room::State& state)
    {
        return TEAMS_Y + TEAM_HEADER_H + Slots(state) * (SLOT_H + SLOT_GAP) + BUTTONS_TOP_GAP + BUTTON_H + BOTTOM_PAD;
    }

    Rect Panel(const Room::State& state)
    {
        return { (REFERENCE_WIDTH - PANEL_W) / 2, PANEL_Y_TOP, PANEL_W, PanelHeight(state) };
    }

    Rect Tab() { return { TAB_X, TAB_Y, TAB_W, TAB_H }; }

    Rect HideButton(const Rect& panel) { return { panel.x + panel.w - 24, panel.y + 4, 20, 18 }; }

    // The rectangle of the n-th visible button, centered in a row.
    Rect ButtonRect(const Room::State& state, int slot)
    {
        const Rect panel = Panel(state);
        const int count = ButtonCount(state);
        const int rowWidth = count * BUTTON_W + (count - 1) * BUTTON_GAP;
        const int top = panel.y + TEAMS_Y + TEAM_HEADER_H + Slots(state) * (SLOT_H + SLOT_GAP) + BUTTONS_TOP_GAP;
        return { panel.x + (panel.w - rowWidth) / 2 + slot * (BUTTON_W + BUTTON_GAP), top, BUTTON_W, BUTTON_H };
    }

    const wchar_t* LabelOf(const Room::State& state, Room::Action action)
    {
        switch (action)
        {
        case Room::Action::SwitchTeam: return L"CAMBIAR EQUIPO";
        case Room::Action::ToggleBots: return state.fillBots ? L"BOTS: SÍ" : L"BOTS: NO";
        case Room::Action::Start: return L"EMPEZAR";
        default: return L"SALIR";
        }
    }

    // Draws the buttons (when asked) and finds the one under the mouse.
    bool ForEachButton(const Room::State& state, Room::Action& hovered, bool draw)
    {
        bool found = false;
        int slot = 0;
        for (const ButtonSpec& spec : BUTTONS)
        {
            if (spec.hostOnly && !state.isHost)
                continue;

            const Rect rect = ButtonRect(state, slot++);
            if (draw)
                Draw::Button(rect, LabelOf(state, spec.action), spec.action != Room::Action::Leave);
            if (!found && Draw::MouseInRect(rect))
            {
                hovered = spec.action;
                found = true;
            }
        }

        return found;
    }

    // The portraits live in the atlas of the minimap (also used by the HUD): loaded here when the room changes, because the
    // minimap only loads it with its own map.
    void EnsurePortraits()
    {
        const unsigned int version = Room::Version();
        if (s_portraitsLoadedFor == version)
            return;

        s_portraitsLoadedFor = version;
        LoadBitmap(L"Interface\\MobaPortraits.tga", CNewUIMiniMap::IMAGE_MINIMAP_MOBA_PORTRAITS, GL_LINEAR, GL_CLAMP_TO_EDGE, false);
    }

    void RenderPortrait(const Rect& frame, int baseClass, bool blue)
    {
        Draw::FillRect(frame, 0.02f, 0.03f, 0.05f, 1.f);
        EndRenderColor();
        if (baseClass >= 0 && baseClass < PORTRAIT_CLASS_COUNT)
        {
            const float cellU = 1.f / ATLAS_COLS;
            const float cellV = 1.f / ATLAS_ROWS;
            RenderBitmap(CNewUIMiniMap::IMAGE_MINIMAP_MOBA_PORTRAITS, static_cast<float>(frame.x), static_cast<float>(frame.y),
                static_cast<float>(frame.w), static_cast<float>(frame.h), (baseClass % ATLAS_COLS) * cellU, (baseClass / ATLAS_COLS) * cellV, cellU, cellV);
        }

        Draw::FrameRect(frame, blue ? 0.43f : 0.92f, blue ? 0.67f : 0.39f, blue ? 0.98f : 0.39f, 1.f);
        Draw::FrameRect({ frame.x + 1, frame.y + 1, frame.w - 2, frame.h - 2 }, blue ? 0.43f : 0.92f, blue ? 0.67f : 0.39f, blue ? 0.98f : 0.39f, 1.f);
        EndRenderColor();
    }

    // One column: the header of the team and a slot per place (portrait and name, or an empty slot).
    void RenderTeam(const Room::State& state, const Rect& panel, const std::vector<Room::Member>& members, bool blue)
    {
        const int columnW = (panel.w - 3 * PAD) / 2;
        const int x = panel.x + PAD + (blue ? 0 : columnW + PAD);
        const int top = panel.y + TEAMS_Y;

        Draw::FillRect({ x, top, columnW, TEAM_HEADER_H }, blue ? 0.10f : 0.22f, blue ? 0.17f : 0.09f, blue ? 0.32f : 0.10f, 1.f);
        EndRenderColor();
        wchar_t header[48] = {};
        swprintf(header, std::size(header), L"%ls   %d / %d", blue ? L"EQUIPO AZUL" : L"EQUIPO ROJO", static_cast<int>(members.size()), state.teamSize);
        Draw::PutTextCentered(Draw::Bold(), x, top + 2, columnW, header, blue ? 140 : 245, blue ? 190 : 130, blue ? 255 : 130);

        for (int i = 0; i < Slots(state); ++i)
        {
            const Rect slot = { x, top + TEAM_HEADER_H + SLOT_GAP + i * (SLOT_H + SLOT_GAP), columnW, SLOT_H };
            const bool taken = i < static_cast<int>(members.size());
            Draw::FillRect(slot, taken && members[i].mine ? 0.20f : 0.07f, taken && members[i].mine ? 0.18f : 0.09f, taken && members[i].mine ? 0.08f : 0.13f, 1.f);
            Draw::FrameRect(slot, 0.23f, 0.27f, 0.35f, 1.f);
            EndRenderColor();
            if (!taken)
            {
                Draw::PutTextLeft(Draw::Body(), slot.x + NAME_X, slot.y + NAME_DY, slot.w - NAME_X, L"(libre)", 90, 98, 112);
                continue;
            }

            const Room::Member& member = members[i];
            RenderPortrait({ slot.x + 3, slot.y + (SLOT_H - PORTRAIT_SIZE) / 2, PORTRAIT_SIZE, PORTRAIT_SIZE }, member.baseClass, blue);
            const std::wstring name = (member.host ? L"★ " : L"") + member.name;
            Draw::PutTextLeft(member.mine ? Draw::Bold() : Draw::Body(), slot.x + NAME_X, slot.y + NAME_DY, slot.w - NAME_X, name.c_str(), 233, 228, member.mine ? 120 : 212);
        }
    }

    void RenderTexts(const Room::State& state, const Rect& panel)
    {
        Draw::PutTextCentered(Draw::Body(), panel.x, panel.y + CODE_LABEL_Y, panel.w, L"SALA PRIVADA  -  CÓDIGO", 170, 179, 194);
        Draw::PutTextCentered(Draw::Title(), panel.x, panel.y + CODE_Y, panel.w, state.code.c_str(), 241, 196, 83);

        wchar_t line[64] = {};
        swprintf(line, std::size(line), L"%ls   -   bots: %ls", state.label.c_str(), state.fillBots ? L"sí" : L"no");
        Draw::PutTextCentered(Draw::Body(), panel.x, panel.y + SUBTITLE_Y, panel.w, line, 233, 228, 212);
        Draw::PutTextCentered(Draw::Bold(), HideButton(panel).x, HideButton(panel).y + 1, HideButton(panel).w, L"_", 233, 228, 212);
    }

    void RenderTab(const Room::State& state)
    {
        Draw::FillRect(Tab(), 0.04f, 0.055f, 0.086f, 1.f);
        Draw::FrameRect(Tab(), 0.78f, 0.64f, 0.29f, Draw::MouseInRect(Tab()) ? 1.f : 0.7f);
        EndRenderColor();
        wchar_t line[48] = {};
        swprintf(line, std::size(line), L"SALA %ls", state.code.c_str());
        Draw::PutTextCentered(Draw::Bold(), Tab().x, Tab().y + 4, Tab().w, line, 241, 196, 83);
    }
}

void CMobaRoomPanel::Create(CNewUIManager* pNewUIMng)
{
    static CMobaRoomPanel s_panel;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_panel);
}

bool CMobaRoomPanel::Render()
{
    const Room::State& state = Room::Get();
    if (!state.present)
        return true;

    const DWORD backColor = g_pRenderText->GetTextColor();
    if (s_collapsed)
    {
        RenderTab(state);
        g_pRenderText->SetTextColor(backColor);
        return true;
    }

    EnsurePortraits();
    const Rect panel = Panel(state);
    Draw::FillRect(panel, 0.04f, 0.055f, 0.086f, 1.f);
    Draw::FrameRect(panel, 0.78f, 0.64f, 0.29f, 1.f);
    EndRenderColor();

    RenderTeam(state, panel, state.blue, true);
    RenderTeam(state, panel, state.red, false);
    Room::Action hovered = Room::Action::Leave;
    ForEachButton(state, hovered, true);
    RenderTexts(state, panel);
    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaRoomPanel::Update()
{
    return true;
}

bool CMobaRoomPanel::UpdateMouseEvent()
{
    if (Draw::HeldClickSwallowed())
        return false;

    const Room::State& state = Room::Get();
    if (!state.present)
    {
        s_collapsed = false;
        return true;
    }

    if (s_collapsed)
    {
        if (!Draw::MouseInRect(Tab()))
            return true;

        if (IsPress(VK_LBUTTON))
        {
            s_collapsed = false;
            Draw::SwallowHeldClick();
        }

        return false;
    }

    if (!Draw::MouseInRect(Panel(state)))
        return true;

    if (IsPress(VK_LBUTTON))
    {
        Room::Action hovered = Room::Action::Leave;
        if (Draw::MouseInRect(HideButton(Panel(state))))
        {
            s_collapsed = true;
            Draw::SwallowHeldClick();
        }
        else if (ForEachButton(state, hovered, false))
        {
            Draw::SwallowHeldClick();
            Room::Request(hovered);
        }
    }

    // The panel swallows the mouse: a click on it must not walk the hero.
    return false;
}

bool CMobaRoomPanel::UpdateKeyEvent()
{
    return true;
}

float CMobaRoomPanel::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaRoomPanel::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
