#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaRoomPanel.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/NewUI/HUD/NewUIMiniMap.h"        // the portrait atlas index
#include "UI/Legacy/UIControls.h"             // g_pRenderText, CUITextInputBox
#include "Core/Utilities/UsefulDef.h"         // DivideStringByPixel
#include "App/Platform/Windows/Winmain.h"     // g_hWnd
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor, RenderBitmap
#include "Render/Textures/ZzzTexture.h"       // LoadBitmap
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaRoom.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace SEASON3B;
namespace Room = World::Moba::Room;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen.
    constexpr int PANEL_W = 606;
    constexpr int PANEL_X = (REFERENCE_WIDTH - PANEL_W) / 2;
    constexpr int PANEL_Y = 24;
    constexpr int PAD = 12;

    constexpr int TITLE_Y = 6;
    constexpr int SUBTITLE_Y = 40;
    constexpr int TEAMS_Y = 64;
    constexpr int TEAM_HEADER_H = 20;
    constexpr int COLUMN_W = 186;
    constexpr int COLUMN_GAP = 8;
    constexpr int SLOT_H = 38;
    constexpr int SLOT_GAP = 4;
    constexpr int PORTRAIT_SIZE = 32;
    constexpr int NAME_X = 42;
    constexpr int NAME_DY = 9;
    constexpr int NAME_DY_STATUS = 3;   // the name sits higher to leave room for the status line
    constexpr int STATUS_DY = 20;
    constexpr int HOST_BUTTON = 17;
    constexpr int HOST_BUTTON_GAP = 2;

    constexpr int CHAT_X = 2 * PAD + 2 * COLUMN_W + COLUMN_GAP;   // from the panel's left edge
    constexpr int CHAT_W = PANEL_W - CHAT_X - PAD;
    constexpr int CHAT_LINE_H = 15;
    constexpr int CHAT_INPUT_H = 24;
    constexpr int CHAT_MAX_CHARS = 100;
    constexpr int CHAT_ROW_CHARS = 96;
    constexpr int CHAT_PIXEL_WIDTH = CHAT_W - 8;
    constexpr int CHAT_ROWS_PER_LINE = 4;

    constexpr int BUTTON_H = 26;
    constexpr int BUTTON_GAP = 6;
    constexpr int BUTTONS_TOP_GAP = 12;
    constexpr int BOTTOM_PAD = 14;
    constexpr int TEXT_DY = 3;

    constexpr int TAB_W = 130;
    constexpr int TAB_H = 24;
    constexpr int TAB_X = 6;
    constexpr int TAB_Y = 44;

    constexpr float LAYER_DEPTH = 10.6f;
    constexpr float KEY_EVENT_ORDER = 100.0f;
    constexpr DWORD UI_KEY_OFFSET = 5;

    // The buttons in the order they are laid out; the host's ones are only there for the host.
    struct ButtonSpec
    {
        Room::Action action;
        bool hostOnly;
    };

    constexpr ButtonSpec BUTTONS[] = {
        { Room::Action::SwitchTeam, false },
        { Room::Action::Ready, false },
        { Room::Action::Skills, false },
        { Room::Action::InviteParty, false },
        { Room::Action::ToggleBots, true },
        { Room::Action::Start, true },
        { Room::Action::CloseRoom, true },
        { Room::Action::Leave, false },
    };

    constexpr int MAX_BUTTONS_PER_ROW = 4;

    bool s_collapsed = false;
    unsigned int s_portraitsLoadedFor = 0;
    std::unique_ptr<CUITextInputBox> s_chatBox;

    // The chat cut into lines which fit the panel, rebuilt when the chat changes.
    struct WrappedLine
    {
        std::wstring text;
        bool notice;
    };

    std::vector<WrappedLine> s_wrapped;
    unsigned int s_wrappedFor = 0;

    // A player picked up with the mouse: dropped on a free place of the other team it changes team (the host can drag anybody,
    // everybody can drag themselves).
    struct DragState
    {
        bool active = false;
        int column = 0;            // 0 blue, 1 red
        std::wstring name;
        unsigned char baseClass = 0;
        bool mine = false;
    };

    DragState s_drag;

    int Slots(const Room::State& state)
    {
        return std::max(state.teamSize, 1);
    }

    // The host's buttons are only there for the host; the host does not get a "ready" button (the host starts).
    bool IsShown(const Room::State& state, const ButtonSpec& spec)
    {
        if (spec.hostOnly && !state.isHost)
            return false;

        return !(spec.action == Room::Action::Ready && state.isHost);
    }

    int ButtonCount(const Room::State& state)
    {
        return static_cast<int>(std::count_if(std::begin(BUTTONS), std::end(BUTTONS), [&](const ButtonSpec& b) { return IsShown(state, b); }));
    }

    // The buttons go in one row, or two balanced rows when there are more than fit comfortably.
    int ButtonRows(const Room::State& state)
    {
        return ButtonCount(state) > MAX_BUTTONS_PER_ROW ? 2 : 1;
    }

    int TeamsBottom(const Room::State& state)
    {
        return TEAMS_Y + TEAM_HEADER_H + SLOT_GAP + Slots(state) * (SLOT_H + SLOT_GAP);
    }

    int PanelHeight(const Room::State& state)
    {
        return TeamsBottom(state) + BUTTONS_TOP_GAP + ButtonRows(state) * BUTTON_H + (ButtonRows(state) - 1) * BUTTON_GAP + BOTTOM_PAD;
    }

    Rect Panel(const Room::State& state) { return { PANEL_X, PANEL_Y, PANEL_W, PanelHeight(state) }; }

    Rect Tab() { return { TAB_X, TAB_Y, TAB_W, TAB_H }; }

    Rect HideButton(const Rect& panel) { return { panel.x + panel.w - 26, panel.y + 6, 20, 18 }; }

    Rect ChatLog(const Rect& panel, const Room::State& state)
    {
        return { panel.x + CHAT_X, panel.y + TEAMS_Y + TEAM_HEADER_H + SLOT_GAP, CHAT_W, TeamsBottom(state) - (TEAMS_Y + TEAM_HEADER_H + SLOT_GAP) - CHAT_INPUT_H - 6 };
    }

    Rect ChatInput(const Rect& panel, const Room::State& state)
    {
        return { panel.x + CHAT_X, panel.y + TeamsBottom(state) - CHAT_INPUT_H - 2, CHAT_W, CHAT_INPUT_H };
    }

    Rect ButtonRect(const Room::State& state, int slot)
    {
        const Rect panel = Panel(state);
        const int count = ButtonCount(state);
        const int rows = ButtonRows(state);
        const int perRow = (count + rows - 1) / rows;
        const int row = slot / perRow;
        const int column = slot % perRow;
        const int width = (panel.w - 2 * PAD - (perRow - 1) * BUTTON_GAP) / perRow;
        return { panel.x + PAD + column * (width + BUTTON_GAP), panel.y + TeamsBottom(state) + BUTTONS_TOP_GAP + row * (BUTTON_H + BUTTON_GAP), width, BUTTON_H };
    }

    // Whether the player has said they are ready.
    bool IAmReady(const Room::State& state)
    {
        for (const std::vector<Room::Member>* team : { &state.blue, &state.red })
        {
            for (const Room::Member& member : *team)
            {
                if (member.mine)
                    return member.ready;
            }
        }

        return false;
    }

    const wchar_t* LabelOf(const Room::State& state, Room::Action action)
    {
        switch (action)
        {
        case Room::Action::SwitchTeam: return L"CAMBIAR EQUIPO";
        case Room::Action::ToggleBots: return state.fillBots ? L"BOTS: S\u00cd" : L"BOTS: NO";
        case Room::Action::Start: return L"EMPEZAR";
        case Room::Action::CloseRoom: return L"CERRAR SALA";
        case Room::Action::Ready: return IAmReady(state) ? L"NO LISTO" : L"LISTO";
        case Room::Action::Skills: return L"MIS SKILLS";
        case Room::Action::InviteParty: return L"INVITAR PARTY";
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
            if (!IsShown(state, spec))
                continue;

            const Rect rect = ButtonRect(state, slot++);
            if (draw)
                Draw::Button(rect, LabelOf(state, spec.action), spec.action == Room::Action::Start || spec.action == Room::Action::SwitchTeam
                    || spec.action == Room::Action::ToggleBots || (spec.action == Room::Action::Ready && !IAmReady(state)));
            if (!found && Draw::MouseInRect(rect))
            {
                hovered = spec.action;
                found = true;
            }
        }

        return found;
    }

    CUITextInputBox& ChatBox()
    {
        if (!s_chatBox)
        {
            s_chatBox = std::make_unique<CUITextInputBox>();
            s_chatBox->Init(g_hWnd, CHAT_W - 8, CHAT_INPUT_H - 4, CHAT_MAX_CHARS);
            s_chatBox->SetTextColor(255, 255, 230, 210);
            s_chatBox->SetBackColor(0, 0, 0, 0);
            s_chatBox->SetState(UISTATE_NORMAL);
            s_chatBox->SetFontProvider(&Draw::Body);
        }

        return *s_chatBox;
    }

    bool ChatFocused()
    {
        return s_chatBox && s_chatBox->HaveFocus();
    }

    // The portraits live in the atlas of the minimap (also used by the HUD): loaded here when the room changes, because the
    // minimap only loads it with its own map.
    void EnsurePortraits()
    {
        const unsigned int version = Room::Version();
        if (s_portraitsLoadedFor == version)
            return;

        s_portraitsLoadedFor = version;
        Draw::LoadPortraits();
    }

    void RenderPortrait(const Rect& frame, int baseClass, bool blue)
    {
        Draw::Portrait(frame, baseClass, blue);
    }

    // The three small buttons of the host on another player's slot: move to the other team, kick, make host.
    Rect HostButton(const Rect& slot, int index)
    {
        return { slot.x + slot.w - (3 - index) * (HOST_BUTTON + HOST_BUTTON_GAP), slot.y + (SLOT_H - HOST_BUTTON) / 2, HOST_BUTTON, HOST_BUTTON };
    }

    const wchar_t* HostButtonLabel(int index)
    {
        static const wchar_t* const labels[3] = { L"<>", L"X", L"*" };
        return labels[index];
    }

    Rect SlotRect(const Room::State& state, const Rect& panel, int column, int i)
    {
        return { panel.x + PAD + column * (COLUMN_W + COLUMN_GAP), panel.y + TEAMS_Y + TEAM_HEADER_H + SLOT_GAP + i * (SLOT_H + SLOT_GAP), COLUMN_W, SLOT_H };
    }

    // The slot of a column under the mouse, or -1.
    int SlotUnderMouse(const Room::State& state, const Rect& panel, int column)
    {
        for (int i = 0; i < Slots(state); ++i)
        {
            if (Draw::MouseInRect(SlotRect(state, panel, column, i)))
                return i;
        }

        return -1;
    }

    // Picks up the player under the mouse (not on one of the host's small buttons), if the player may move them.
    bool BeginDrag(const Room::State& state, const Rect& panel)
    {
        for (int column = 0; column < 2; ++column)
        {
            const std::vector<Room::Member>& members = column == 0 ? state.blue : state.red;
            const int slot = SlotUnderMouse(state, panel, column);
            if (slot < 0 || slot >= static_cast<int>(members.size()))
                continue;

            const Room::Member& member = members[slot];
            if (!state.isHost && !member.mine)
                return false;

            s_drag = { true, column, member.name, member.baseClass, member.mine };
            return true;
        }

        return false;
    }

    // The mouse button was released: if it is over a free place of the other team, the player changes team.
    void EndDrag(const Room::State& state, const Rect& panel)
    {
        const DragState drag = s_drag;
        s_drag = DragState{};
        const int target = 1 - drag.column;
        const std::vector<Room::Member>& members = target == 0 ? state.blue : state.red;
        const int slot = SlotUnderMouse(state, panel, target);
        if (slot < static_cast<int>(members.size()) || slot < 0)
            return;

        if (drag.mine)
            Room::Request(Room::Action::SwitchTeam);
        else
            Room::RequestOnPlayer(Room::TargetAction::Move, drag.name);
    }

    // While dragging: the free places of the other team light up and the player follows the mouse.
    void RenderDragTargets(const Room::State& state, const Rect& panel)
    {
        if (!s_drag.active)
            return;

        const int target = 1 - s_drag.column;
        const int taken = static_cast<int>((target == 0 ? state.blue : state.red).size());
        for (int i = taken; i < Slots(state); ++i)
        {
            Draw::FrameRect(SlotRect(state, panel, target, i), 0.95f, 0.80f, 0.40f, 1.f);
            Draw::FrameRect({ SlotRect(state, panel, target, i).x + 1, SlotRect(state, panel, target, i).y + 1, COLUMN_W - 2, SLOT_H - 2 }, 0.95f, 0.80f, 0.40f, 1.f);
        }

        EndRenderColor();
    }

    void RenderDragGhost()
    {
        if (!s_drag.active)
            return;

        const Rect ghost = { static_cast<int>(MouseX) - 16, static_cast<int>(MouseY) - 16, COLUMN_W - 40, SLOT_H - 4 };
        Draw::FillRect(ghost, 0.20f, 0.18f, 0.08f, 1.f);
        Draw::FrameRect(ghost, 0.95f, 0.80f, 0.40f, 1.f);
        EndRenderColor();
        RenderPortrait({ ghost.x + 2, ghost.y + 2, PORTRAIT_SIZE - 4, PORTRAIT_SIZE - 4 }, s_drag.baseClass, s_drag.column == 0);
        Draw::PutTextLeft(Draw::Bold(), ghost.x + NAME_X - 8, ghost.y + NAME_DY - 3, ghost.w - NAME_X, s_drag.name.c_str(), 255, 244, 214);
    }

    // What a button of the panel does, for the tooltip.
    const wchar_t* ButtonHint(Room::Action action)
    {
        switch (action)
        {
        case Room::Action::SwitchTeam: return L"Pasarte al otro equipo (tambi\u00e9n puedes arrastrarte a un lugar libre)";
        case Room::Action::ToggleBots: return L"Rellenar o no con bots los lugares libres";
        case Room::Action::Start: return L"Empezar la partida con los jugadores de la sala";
        case Room::Action::CloseRoom: return L"Cerrar la sala y sacar a todos";
        case Room::Action::Ready: return L"Avisar que est\u00e1s listo (el anfitri\u00f3n empieza cuando todos lo est\u00e9n)";
        case Room::Action::Skills: return L"Elegir las 4 skills de la partida";
        case Room::Action::InviteParty: return L"Invitar a la sala a los miembros de tu party (tambi\u00e9n: /invitar nombre)";
        default: return L"Salir de la sala";
        }
    }

    const wchar_t* HostButtonHint(int index)
    {
        static const wchar_t* const hints[3] = {
            L"Mover al otro equipo (tambi\u00e9n puedes arrastrarlo)",
            L"Expulsar de la sala (no podr\u00e1 volver)",
            L"Hacer anfitri\u00f3n a este jugador" };
        return hints[index];
    }

    // The tooltip of whatever small button or panel button the mouse rests on.
    void RenderTooltips(const Room::State& state, const Rect& panel)
    {
        if (s_drag.active)
            return;

        Room::Action hovered = Room::Action::Leave;
        if (ForEachButton(state, hovered, false))
        {
            Draw::Tooltip(ButtonHint(hovered));
            return;
        }

        if (!state.isHost)
            return;

        for (int column = 0; column < 2; ++column)
        {
            const std::vector<Room::Member>& members = column == 0 ? state.blue : state.red;
            for (int i = 0; i < static_cast<int>(members.size()); ++i)
            {
                for (int b = 0; b < 3 && !members[i].mine; ++b)
                {
                    if (Draw::MouseInRect(HostButton(SlotRect(state, panel, column, i), b)))
                    {
                        Draw::Tooltip(HostButtonHint(b));
                        return;
                    }
                }
            }
        }
    }

    // One column: the header of the team and a slot per place (portrait and name, or an empty slot).
    void RenderTeam(const Room::State& state, const Rect& panel, const std::vector<Room::Member>& members, bool blue)
    {
        const int column = blue ? 0 : 1;
        const int x = panel.x + PAD + column * (COLUMN_W + COLUMN_GAP);
        const int top = panel.y + TEAMS_Y;

        Draw::FillRect({ x, top, COLUMN_W, TEAM_HEADER_H }, blue ? 0.10f : 0.22f, blue ? 0.17f : 0.09f, blue ? 0.32f : 0.10f, 1.f);
        EndRenderColor();
        wchar_t header[48] = {};
        swprintf(header, std::size(header), L"%ls   %d / %d", blue ? L"EQUIPO AZUL" : L"EQUIPO ROJO", static_cast<int>(members.size()), state.teamSize);
        Draw::PutTextCentered(Draw::Bold(), x, top + 2, COLUMN_W, header, blue ? 140 : 245, blue ? 190 : 130, blue ? 255 : 130);

        for (int i = 0; i < Slots(state); ++i)
        {
            const Rect slot = SlotRect(state, panel, column, i);
            const bool taken = i < static_cast<int>(members.size());
            const bool mine = taken && members[i].mine;
            Draw::FillRect(slot, mine ? 0.20f : 0.07f, mine ? 0.18f : 0.09f, mine ? 0.08f : 0.13f, 1.f);
            Draw::FrameRect(slot, 0.23f, 0.27f, 0.35f, 1.f);
            EndRenderColor();
            if (!taken)
            {
                Draw::PutTextLeft(Draw::Body(), slot.x + NAME_X, slot.y + NAME_DY, slot.w - NAME_X, L"(libre)", 90, 98, 112);
                continue;
            }

            const Room::Member& member = members[i];
            RenderPortrait({ slot.x + 3, slot.y + (SLOT_H - PORTRAIT_SIZE) / 2, PORTRAIT_SIZE, PORTRAIT_SIZE }, member.baseClass, blue);
            const int nameWidth = slot.w - NAME_X - (state.isHost && !member.mine ? 3 * (HOST_BUTTON + HOST_BUTTON_GAP) : 4);
            const std::wstring name = (member.host ? L"* " : L"") + member.name;
            Draw::PutTextLeft(mine ? Draw::Bold() : Draw::Body(), slot.x + NAME_X, slot.y + NAME_DY_STATUS, nameWidth, name.c_str(), 233, 228, mine ? 120 : 212);

            // Second line: ready (the host does not need it) and whether the 4 skills were picked.
            const bool good = member.host || member.ready;
            const std::wstring status = std::wstring(member.host ? L"Anfitri\u00f3n" : (member.ready ? L"LISTO" : L"No listo")) + (member.hasSkills ? L" - skills OK" : L" - sin skills");
            Draw::PutTextLeft(Draw::Body(), slot.x + NAME_X, slot.y + STATUS_DY, nameWidth, status.c_str(), good ? 111 : 170, good ? 211 : 179, good ? 154 : 194);

            if (state.isHost && !member.mine)
            {
                for (int b = 0; b < 3; ++b)
                {
                    const Rect rect = HostButton(slot, b);
                    const bool hot = Draw::MouseInRect(rect);
                    Draw::FillRect(rect, b == 1 ? (hot ? 0.45f : 0.30f) : (hot ? 0.20f : 0.12f), b == 1 ? 0.12f : (hot ? 0.34f : 0.22f), b == 1 ? 0.12f : (hot ? 0.20f : 0.12f), 1.f);
                    Draw::FrameRect(rect, 0.80f, 0.66f, 0.30f, hot ? 1.f : 0.6f);
                    EndRenderColor();
                    Draw::PutTextCentered(Draw::Bold(), rect.x, rect.y + 1, rect.w, HostButtonLabel(b), 255, 244, 214);
                }
            }
        }
    }

    // The chat cut into lines; rebuilt only when the chat changed (the font has to be set by the caller).
    void RewrapChat()
    {
        if (s_wrappedFor == Room::ChatVersion())
            return;

        s_wrappedFor = Room::ChatVersion();
        s_wrapped.clear();
        g_pRenderText->SetFont(Draw::Body());
        for (const Room::ChatLine& line : Room::Chat())
        {
            const std::wstring full = line.notice ? L"* " + line.text : line.name + L": " + line.text;
            wchar_t rows[CHAT_ROWS_PER_LINE][CHAT_ROW_CHARS] = {};
            const int count = ::DivideStringByPixel(&rows[0][0], CHAT_ROWS_PER_LINE, CHAT_ROW_CHARS, full.c_str(), CHAT_PIXEL_WIDTH, false);
            for (int i = 0; i < count && i < CHAT_ROWS_PER_LINE; ++i)
                s_wrapped.push_back({ rows[i], line.notice });
        }
    }

    void RenderChat(const Room::State& state, const Rect& panel)
    {
        const Rect log = ChatLog(panel, state);
        Draw::FillRect({ log.x, panel.y + TEAMS_Y, log.w, TEAM_HEADER_H }, 0.09f, 0.11f, 0.16f, 1.f);
        Draw::FillRect(log, 0.02f, 0.03f, 0.05f, 1.f);
        Draw::FrameRect(log, 0.23f, 0.27f, 0.35f, 1.f);
        const Rect input = ChatInput(panel, state);
        Draw::FillRect(input, 0.02f, 0.03f, 0.05f, 1.f);
        Draw::FrameRect(input, ChatFocused() ? 0.95f : 0.78f, ChatFocused() ? 0.80f : 0.64f, ChatFocused() ? 0.40f : 0.29f, 1.f);
        EndRenderColor();

        Draw::PutTextCentered(Draw::Bold(), log.x, panel.y + TEAMS_Y + 2, log.w, L"CHAT DE LA SALA", 170, 179, 194);
        RewrapChat();
        const int visible = std::max(1, (log.h - 6) / CHAT_LINE_H);
        const int first = std::max(0, static_cast<int>(s_wrapped.size()) - visible);
        for (int i = first; i < static_cast<int>(s_wrapped.size()); ++i)
        {
            const WrappedLine& line = s_wrapped[i];
            Draw::PutTextLeft(Draw::Body(), log.x + 4, log.y + 3 + (i - first) * CHAT_LINE_H, log.w - 6, line.text.c_str(),
                line.notice ? 241 : 233, line.notice ? 196 : 228, line.notice ? 83 : 212);
        }

        ChatBox().SetPosition(input.x + 6, input.y + (input.h - Draw::BodyLineHeight()) / 2);
        ChatBox().Render();
        if (!ChatFocused() && ChatBox().GetState() == UISTATE_NORMAL)
        {
            wchar_t text[8] = {};
            ChatBox().GetText(text, 2);
            if (text[0] == L'\0')
                Draw::PutTextLeft(Draw::Body(), input.x + 6, input.y + (input.h - Draw::BodyLineHeight()) / 2, input.w - 8, L"Pulsa Intro para escribir...", 90, 98, 112);
        }
    }

    void RenderTexts(const Room::State& state, const Rect& panel)
    {
        Draw::PutTextCentered(Draw::Title(), panel.x, panel.y + TITLE_Y, panel.w, state.name.c_str(), 241, 196, 83);
        const std::wstring subtitle = Room::Label(state) + (state.fillBots ? L"   -   con bots" : L"   -   sin bots") + (state.locked ? L"   -   con clave" : L"");
        Draw::PutTextCentered(Draw::Body(), panel.x, panel.y + SUBTITLE_Y, panel.w, subtitle.c_str(), 233, 228, 212);
        Draw::PutTextCentered(Draw::Bold(), HideButton(panel).x, HideButton(panel).y + 1, HideButton(panel).w, L"_", 233, 228, 212);
    }

    void RenderTab(const Room::State& state)
    {
        Draw::FillRect(Tab(), 0.04f, 0.055f, 0.086f, 1.f);
        Draw::FrameRect(Tab(), 0.78f, 0.64f, 0.29f, Draw::MouseInRect(Tab()) ? 1.f : 0.7f);
        EndRenderColor();
        Draw::PutTextCentered(Draw::Bold(), Tab().x, Tab().y + 4, Tab().w, L"MI SALA", 241, 196, 83);
    }

    void SendChatLine()
    {
        wchar_t text[CHAT_MAX_CHARS + 2] = {};
        ChatBox().GetText(text, CHAT_MAX_CHARS + 1);
        if (text[0] != L'\0')
            Room::SendChat(text);

        ChatBox().SetText(nullptr);
    }

    // A click on one of the host's three small buttons of a slot.
    bool HostButtonClick(const Room::State& state, const Rect& panel)
    {
        if (!state.isHost)
            return false;

        for (int column = 0; column < 2; ++column)
        {
            const std::vector<Room::Member>& members = column == 0 ? state.blue : state.red;
            for (int i = 0; i < static_cast<int>(members.size()); ++i)
            {
                if (members[i].mine)
                    continue;

                for (int b = 0; b < 3; ++b)
                {
                    if (!Draw::MouseInRect(HostButton(SlotRect(state, panel, column, i), b)))
                        continue;

                    static constexpr Room::TargetAction actions[3] = { Room::TargetAction::Move, Room::TargetAction::Kick, Room::TargetAction::MakeHost };
                    Room::RequestOnPlayer(actions[b], members[i].name);
                    return true;
                }
            }
        }

        return false;
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
    RenderChat(state, panel);
    RenderDragTargets(state, panel);
    Room::Action hovered = Room::Action::Leave;
    ForEachButton(state, hovered, true);
    RenderTexts(state, panel);
    RenderTooltips(state, panel);
    RenderDragGhost();
    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaRoomPanel::Update()
{
    const Room::State& state = Room::Get();
    if (!state.present)
    {
        if (ChatFocused())
            CUITextInputBox::ReleaseFocus();

        s_drag = DragState{};
        s_collapsed = false;
        SetRelatedWnd(g_hWnd);
        return true;
    }

    if (s_collapsed && ChatFocused())
        CUITextInputBox::ReleaseFocus();

    // Keys go to the window which owns the focused text field.
    SetRelatedWnd(ChatFocused() ? ChatBox().GetHandle() : g_hWnd);
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

    const Rect panel = Panel(state);
    if (s_drag.active)
    {
        if (!(IsPress(VK_LBUTTON) || IsRepeat(VK_LBUTTON)))
            EndDrag(state, panel);

        return false;
    }

    if (!Draw::MouseInRect(panel))
        return true;

    ChatBox().DoAction();
    if (IsPress(VK_LBUTTON))
    {
        Room::Action hovered = Room::Action::Leave;
        if (Draw::MouseInRect(HideButton(panel)))
        {
            s_collapsed = true;
            Draw::SwallowHeldClick();
        }
        else if (Draw::MouseInRect(ChatInput(panel, state)))
        {
            ChatBox().GiveFocus();
            Draw::SwallowHeldClick();
        }
        else if (HostButtonClick(state, panel))
        {
            Draw::SwallowHeldClick();
        }
        else if (BeginDrag(state, panel))
        {
            // the press is kept: the drop is decided when the button is released
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
    const Room::State& state = Room::Get();
    if (!state.present || s_collapsed)
        return true;

    if (ChatFocused())
    {
        if (IsPress(VK_RETURN))
        {
            SendChatLine();
            return false;
        }

        if (IsPress(VK_ESCAPE))
        {
            CUITextInputBox::ReleaseFocus();
            return false;
        }

        return true;
    }

    if (IsPress(VK_RETURN))
    {
        ChatBox().GiveFocus();
        return false;
    }

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
