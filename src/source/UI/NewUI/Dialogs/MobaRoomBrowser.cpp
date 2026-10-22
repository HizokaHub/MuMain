#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaRoomBrowser.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/Legacy/UIControls.h"             // g_pRenderText, CUITextInputBox
#include "App/Platform/Windows/Winmain.h"     // g_hWnd
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaRoom.h"
#include "World/Moba/MobaRoomList.h"

#include <algorithm>
#include <chrono>
#include <memory>

extern int MouseWheel;

using namespace SEASON3B;
namespace RoomList = World::Moba::RoomList;
namespace Room = World::Moba::Room;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen.
    constexpr int PANEL_W = 590;
    constexpr int PANEL_H = 372;
    constexpr int PANEL_X = (REFERENCE_WIDTH - PANEL_W) / 2;
    constexpr int PANEL_Y = 40;
    constexpr int PAD = 12;
    constexpr int TITLE_Y = 6;
    constexpr int HEADER_Y = 46;
    constexpr int ROWS_Y = 66;
    constexpr int ROW_H = 26;
    constexpr int VISIBLE_ROWS = 9;
    constexpr int TEXT_DY = 4;
    constexpr int BUTTON_W = 150;
    constexpr int BUTTON_H = 26;
    constexpr int BUTTON_GAP = 12;
    constexpr int BUTTONS_BOTTOM = 14;
    constexpr int JOIN_W = 82;
    constexpr int JOIN_H = 20;
    constexpr int SCROLL_W = 16;

    // The columns of the list: left edge from the panel and width.
    struct Column { int x; int w; const wchar_t* title; };
    constexpr Column COL_NAME = { PAD, 156, L"Sala" };
    constexpr Column COL_HOST = { 172, 84, L"Anfitri\u00f3n" };
    constexpr Column COL_FORMAT = { 258, 126, L"Formato" };
    constexpr Column COL_PLAYERS = { 386, 52, L"Jugad." };
    constexpr Column COL_TIME = { 440, 52, L"Creada" };
    constexpr int JOIN_X = 494;

    // The create form.
    constexpr int FORM_LABEL_X = 30;
    constexpr int FORM_FIELD_X = 200;
    constexpr int FORM_NAME_Y = 70;
    constexpr int FORM_SIZE_Y = 112;
    constexpr int FORM_MAP_Y = 160;
    constexpr int FORM_BOTS_Y = 208;
    constexpr int FORM_SUMMARY_Y = 262;
    constexpr int NAME_BOX_W = 330;
    constexpr int NAME_BOX_H = 22;
    constexpr int NAME_MAX_LENGTH = 24;
    constexpr int SIZE_BUTTON_W = 44;
    constexpr int CHOICE_BUTTON_W = 160;
    constexpr int CHOICE_H = 26;
    constexpr int CHOICE_GAP = 8;

    constexpr float LAYER_DEPTH = 10.55f;
    constexpr float KEY_EVENT_ORDER = 90.0f;
    constexpr DWORD UI_KEY_OFFSET = 7;

    constexpr double REFRESH_EVERY_MS = 3000.0;
    constexpr int SECONDS_PER_MINUTE = 60;
    constexpr int WARNING_IDLE_SECONDS = 12 * SECONDS_PER_MINUTE;   // the server warns the room at this point
    constexpr int MAX_TEAM_SIZE = 5;

    enum class Mode { List, Form };

    struct FormState
    {
        int teamSize = 3;
        bool midOnly = false;
        bool bots = true;
    };

    Mode s_mode = Mode::List;
    FormState s_form;
    int s_scroll = 0;
    unsigned int s_seenVersion = 0;
    double s_lastRefreshMs = 0.0;
    bool s_wasOpen = false;
    std::unique_ptr<CUITextInputBox> s_nameBox;

    double NowMs()
    {
        using namespace std::chrono;
        return static_cast<double>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }

    Rect Panel() { return { PANEL_X, PANEL_Y, PANEL_W, PANEL_H }; }

    Rect FooterButton(int slot, int count)
    {
        const int rowWidth = count * BUTTON_W + (count - 1) * BUTTON_GAP;
        return { PANEL_X + (PANEL_W - rowWidth) / 2 + slot * (BUTTON_W + BUTTON_GAP), PANEL_Y + PANEL_H - BUTTONS_BOTTOM - BUTTON_H, BUTTON_W, BUTTON_H };
    }

    Rect RowRect(int row) { return { PANEL_X + PAD, PANEL_Y + ROWS_Y + row * ROW_H, PANEL_W - 2 * PAD - SCROLL_W - 4, ROW_H - 2 }; }

    Rect JoinButton(int row) { return { PANEL_X + JOIN_X, PANEL_Y + ROWS_Y + row * ROW_H + 3, JOIN_W, JOIN_H }; }

    Rect ScrollButton(bool up)
    {
        const int listHeight = VISIBLE_ROWS * ROW_H;
        return { PANEL_X + PANEL_W - PAD - SCROLL_W, PANEL_Y + ROWS_Y + (up ? 0 : listHeight - SCROLL_W), SCROLL_W, SCROLL_W };
    }

    Rect SizeButton(int size) { return { PANEL_X + FORM_FIELD_X + (size - 1) * (SIZE_BUTTON_W + CHOICE_GAP), PANEL_Y + FORM_SIZE_Y, SIZE_BUTTON_W, CHOICE_H }; }

    Rect ChoiceButton(int y, int index) { return { PANEL_X + FORM_FIELD_X + index * (CHOICE_BUTTON_W + CHOICE_GAP), PANEL_Y + y, CHOICE_BUTTON_W, CHOICE_H }; }

    int MaxScroll(const RoomList::State& state)
    {
        return std::max(0, static_cast<int>(state.rooms.size()) - VISIBLE_ROWS);
    }

    std::wstring FormatText(const RoomList::Entry& room)
    {
        return std::to_wstring(room.teamSize) + L"v" + std::to_wstring(room.teamSize) + (room.midOnly ? L" central" : L" completa") + (room.bots ? L" +bots" : L"");
    }

    std::wstring AgeText(int seconds)
    {
        if (seconds < SECONDS_PER_MINUTE)
            return L"<1 min";
        return std::to_wstring(seconds / SECONDS_PER_MINUTE) + L" min";
    }

    // A button which shows whether it is the picked choice of its group.
    void ChoiceDraw(const Rect& rect, const wchar_t* label, bool picked)
    {
        Draw::FillRect(rect, picked ? 0.14f : 0.07f, picked ? 0.30f : 0.09f, picked ? 0.14f : 0.13f, 1.f);
        const bool hot = Draw::MouseInRect(rect);
        if (picked)
            Draw::FrameRect(rect, 0.95f, 0.80f, 0.40f, 1.f);
        else
            Draw::FrameRect(rect, hot ? 0.60f : 0.30f, hot ? 0.64f : 0.34f, hot ? 0.72f : 0.42f, 1.f);
        EndRenderColor();
        Draw::PutTextCentered(picked ? Draw::Bold() : Draw::Body(), rect.x, rect.y + 5, rect.w, label, picked ? 255 : 200, picked ? 244 : 205, picked ? 214 : 214);
    }

    CUITextInputBox& NameBox()
    {
        if (!s_nameBox)
        {
            s_nameBox = std::make_unique<CUITextInputBox>();
            s_nameBox->Init(g_hWnd, NAME_BOX_W, NAME_BOX_H, NAME_MAX_LENGTH);
            s_nameBox->SetTextColor(255, 255, 230, 210);
            s_nameBox->SetBackColor(0, 0, 0, 0);
            s_nameBox->SetState(UISTATE_NORMAL);
            s_nameBox->SetFontProvider(&Draw::Body);
        }

        return *s_nameBox;
    }

    void OpenForm()
    {
        s_mode = Mode::Form;
        CUITextInputBox& box = NameBox();
        box.SetText(nullptr);
        box.GiveFocus();
    }

    void CloseBrowser()
    {
        if (s_nameBox && s_nameBox->HaveFocus())
            CUITextInputBox::ReleaseFocus();

        s_mode = Mode::List;
        RoomList::Close();
    }

    void SubmitForm()
    {
        wchar_t name[NAME_MAX_LENGTH + 2] = {};
        NameBox().GetText(name, NAME_MAX_LENGTH + 1);
        RoomList::Create(name, s_form.teamSize, s_form.midOnly, s_form.bots);
        CUITextInputBox::ReleaseFocus();
    }

    void RenderHeader()
    {
        const int y = PANEL_Y + HEADER_Y;
        Draw::FillRect({ PANEL_X + PAD, y, PANEL_W - 2 * PAD, ROW_H - 6 }, 0.09f, 0.11f, 0.16f, 1.f);
        EndRenderColor();
        for (const Column* column : { &COL_NAME, &COL_HOST, &COL_FORMAT, &COL_PLAYERS, &COL_TIME })
            Draw::PutTextLeft(Draw::Bold(), PANEL_X + column->x + 2, y + 2, column->w, column->title, 170, 179, 194);
    }

    void RenderRow(const RoomList::Entry& room, int row)
    {
        const bool full = room.players >= 2 * room.teamSize;
        const bool stale = room.idleSeconds >= WARNING_IDLE_SECONDS;
        const Rect rect = RowRect(row);
        Draw::FillRect(rect, Draw::MouseInRect(rect) ? 0.12f : 0.07f, Draw::MouseInRect(rect) ? 0.15f : 0.09f, Draw::MouseInRect(rect) ? 0.22f : 0.13f, 1.f);
        EndRenderColor();

        const int y = rect.y + TEXT_DY;
        const BYTE tint = stale ? 190 : 233;
        Draw::PutTextLeft(Draw::Bold(), PANEL_X + COL_NAME.x + 2, y, COL_NAME.w, room.name.c_str(), 241, 196, 83);
        Draw::PutTextLeft(Draw::Body(), PANEL_X + COL_HOST.x, y, COL_HOST.w, room.host.c_str(), tint, tint, tint);
        Draw::PutTextLeft(Draw::Body(), PANEL_X + COL_FORMAT.x, y, COL_FORMAT.w, FormatText(room).c_str(), tint, tint, tint);

        wchar_t players[16] = {};
        swprintf(players, std::size(players), L"%d/%d", room.players, 2 * room.teamSize);
        Draw::PutTextLeft(Draw::Body(), PANEL_X + COL_PLAYERS.x, y, COL_PLAYERS.w, players, full ? 235 : 111, full ? 100 : 211, full ? 100 : 154);
        Draw::PutTextLeft(Draw::Body(), PANEL_X + COL_TIME.x, y, COL_TIME.w, AgeText(room.ageSeconds).c_str(), stale ? 235 : tint, stale ? 150 : tint, stale ? 100 : tint);

        const Rect join = JoinButton(row);
        if (full)
        {
            Draw::FillRect(join, 0.15f, 0.16f, 0.19f, 1.f);
            Draw::FrameRect(join, 0.40f, 0.40f, 0.40f, 1.f);
            EndRenderColor();
            Draw::PutTextCentered(Draw::Bold(), join.x, join.y + 1, join.w, L"LLENA", 110, 110, 110);
        }
        else
        {
            Draw::Button(join, L"UNIRME", true);
        }
    }

    void RenderList(const RoomList::State& state)
    {
        RenderHeader();
        if (state.rooms.empty())
        {
            Draw::PutTextCentered(Draw::Body(), PANEL_X, PANEL_Y + ROWS_Y + 90, PANEL_W, L"No hay salas abiertas. \u00a1Crea la primera!", 170, 179, 194);
        }

        for (int row = 0; row < VISIBLE_ROWS && s_scroll + row < static_cast<int>(state.rooms.size()); ++row)
            RenderRow(state.rooms[s_scroll + row], row);

        if (MaxScroll(state) > 0)
        {
            for (const bool up : { true, false })
            {
                const Rect rect = ScrollButton(up);
                Draw::FillRect(rect, 0.15f, 0.17f, 0.22f, 1.f);
                Draw::FrameRect(rect, 0.60f, 0.64f, 0.72f, 1.f);
                EndRenderColor();
                Draw::PutTextCentered(Draw::Bold(), rect.x, rect.y, rect.w, up ? L"^" : L"v", 233, 228, 212);
            }
        }

        Draw::Button(FooterButton(0, 3), L"CREAR SALA", true);
        Draw::Button(FooterButton(1, 3), L"ACTUALIZAR", true);
        Draw::Button(FooterButton(2, 3), L"CERRAR", false);
    }

    void RenderFormTexts()
    {
        Draw::PutTextLeft(Draw::Bold(), PANEL_X + FORM_LABEL_X, PANEL_Y + FORM_NAME_Y + 3, FORM_FIELD_X - FORM_LABEL_X, L"Nombre de la sala", 233, 228, 212);
        Draw::PutTextLeft(Draw::Bold(), PANEL_X + FORM_LABEL_X, PANEL_Y + FORM_SIZE_Y + 5, FORM_FIELD_X - FORM_LABEL_X, L"Jugadores por equipo", 233, 228, 212);
        Draw::PutTextLeft(Draw::Bold(), PANEL_X + FORM_LABEL_X, PANEL_Y + FORM_MAP_Y + 5, FORM_FIELD_X - FORM_LABEL_X, L"Mapa", 233, 228, 212);
        Draw::PutTextLeft(Draw::Bold(), PANEL_X + FORM_LABEL_X, PANEL_Y + FORM_BOTS_Y + 5, FORM_FIELD_X - FORM_LABEL_X, L"Bots", 233, 228, 212);

        wchar_t summary[160] = {};
        swprintf(summary, std::size(summary), L"Se jugar\u00e1 %dv%d %ls, %ls.", s_form.teamSize, s_form.teamSize,
            s_form.midOnly ? L"solo por el carril central (sin jungla)" : L"en los 3 carriles con jungla",
            s_form.bots ? L"rellenando con bots los lugares libres" : L"solo con jugadores (los equipos tienen que quedar iguales)");
        Draw::PutTextCentered(Draw::Body(), PANEL_X, PANEL_Y + FORM_SUMMARY_Y, PANEL_W, summary, 170, 179, 194);
    }

    void RenderForm()
    {
        const Rect nameFrame = { PANEL_X + FORM_FIELD_X, PANEL_Y + FORM_NAME_Y, NAME_BOX_W + 12, NAME_BOX_H + 8 };
        Draw::FillRect(nameFrame, 0.02f, 0.03f, 0.05f, 1.f);
        Draw::FrameRect(nameFrame, NameBox().HaveFocus() ? 0.95f : 0.78f, NameBox().HaveFocus() ? 0.80f : 0.64f, NameBox().HaveFocus() ? 0.40f : 0.29f, 1.f);
        EndRenderColor();

        for (int size = 1; size <= MAX_TEAM_SIZE; ++size)
            ChoiceDraw(SizeButton(size), std::to_wstring(size).c_str(), s_form.teamSize == size);

        ChoiceDraw(ChoiceButton(FORM_MAP_Y, 0), L"3 carriles + jungla", !s_form.midOnly);
        ChoiceDraw(ChoiceButton(FORM_MAP_Y, 1), L"Solo carril central", s_form.midOnly);
        ChoiceDraw(ChoiceButton(FORM_BOTS_Y, 0), L"Con bots", s_form.bots);
        ChoiceDraw(ChoiceButton(FORM_BOTS_Y, 1), L"Sin bots", !s_form.bots);

        Draw::Button(FooterButton(0, 2), L"CREAR SALA", true);
        Draw::Button(FooterButton(1, 2), L"VOLVER", false);
        RenderFormTexts();

        NameBox().SetPosition(nameFrame.x + 8, nameFrame.y + (nameFrame.h - Draw::BodyLineHeight()) / 2);
        NameBox().Render();
    }

    void ListClick(const RoomList::State& state)
    {
        if (Draw::MouseInRect(FooterButton(0, 3)))
        {
            OpenForm();
            return;
        }

        if (Draw::MouseInRect(FooterButton(1, 3)))
        {
            RoomList::Refresh();
            s_lastRefreshMs = NowMs();
            return;
        }

        if (Draw::MouseInRect(FooterButton(2, 3)))
        {
            CloseBrowser();
            return;
        }

        if (MaxScroll(state) > 0 && Draw::MouseInRect(ScrollButton(true)))
            s_scroll = std::max(0, s_scroll - 1);
        else if (MaxScroll(state) > 0 && Draw::MouseInRect(ScrollButton(false)))
            s_scroll = std::min(MaxScroll(state), s_scroll + 1);

        for (int row = 0; row < VISIBLE_ROWS && s_scroll + row < static_cast<int>(state.rooms.size()); ++row)
        {
            const RoomList::Entry& room = state.rooms[s_scroll + row];
            if (Draw::MouseInRect(JoinButton(row)) && room.players < 2 * room.teamSize)
                RoomList::Join(room.id);
        }
    }

    void FormClick()
    {
        for (int size = 1; size <= MAX_TEAM_SIZE; ++size)
        {
            if (Draw::MouseInRect(SizeButton(size)))
                s_form.teamSize = size;
        }

        if (Draw::MouseInRect(ChoiceButton(FORM_MAP_Y, 0)))
            s_form.midOnly = false;
        else if (Draw::MouseInRect(ChoiceButton(FORM_MAP_Y, 1)))
            s_form.midOnly = true;
        else if (Draw::MouseInRect(ChoiceButton(FORM_BOTS_Y, 0)))
            s_form.bots = true;
        else if (Draw::MouseInRect(ChoiceButton(FORM_BOTS_Y, 1)))
            s_form.bots = false;
        else if (Draw::MouseInRect(FooterButton(0, 2)))
            SubmitForm();
        else if (Draw::MouseInRect(FooterButton(1, 2)))
            s_mode = Mode::List;
    }
}

void CMobaRoomBrowser::Create(CNewUIManager* pNewUIMng)
{
    static CMobaRoomBrowser s_browser;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_browser);
}

bool CMobaRoomBrowser::Render()
{
    const RoomList::State& state = RoomList::Get();
    if (!state.open || Room::Get().present)
        return true;

    const DWORD backColor = g_pRenderText->GetTextColor();
    Draw::FillRect(Panel(), 0.04f, 0.055f, 0.086f, 1.f);
    Draw::FrameRect(Panel(), 0.78f, 0.64f, 0.29f, 1.f);
    EndRenderColor();

    Draw::PutTextCentered(Draw::Title(), PANEL_X, PANEL_Y + TITLE_Y, PANEL_W, s_mode == Mode::Form ? L"CREAR SALA" : L"SALAS PRIVADAS", 241, 196, 83);
    if (s_mode == Mode::Form)
        RenderForm();
    else
        RenderList(state);

    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaRoomBrowser::Update()
{
    const RoomList::State& state = RoomList::Get();
    const bool visible = state.open && !Room::Get().present;
    if (!visible)
    {
        if (s_wasOpen && s_nameBox && s_nameBox->HaveFocus())
            CUITextInputBox::ReleaseFocus();

        s_wasOpen = false;
        s_mode = Mode::List;
        SetRelatedWnd(g_hWnd);
        return true;
    }

    if (!s_wasOpen)
    {
        s_wasOpen = true;
        s_scroll = 0;
        s_lastRefreshMs = NowMs();
    }

    s_scroll = std::clamp(s_scroll, 0, MaxScroll(state));
    if (s_mode == Mode::List && NowMs() - s_lastRefreshMs > REFRESH_EVERY_MS)
    {
        RoomList::Refresh();
        s_lastRefreshMs = NowMs();
    }

    // Keys go to the window which owns the focused text field.
    SetRelatedWnd(s_nameBox && s_nameBox->HaveFocus() ? s_nameBox->GetHandle() : g_hWnd);
    return true;
}

bool CMobaRoomBrowser::UpdateMouseEvent()
{
    if (Draw::HeldClickSwallowed())
        return false;

    const RoomList::State& state = RoomList::Get();
    if (!state.open || Room::Get().present || !Draw::MouseInRect(Panel()))
        return true;

    if (s_mode == Mode::Form)
        NameBox().DoAction();

    if (MouseWheel != 0 && s_mode == Mode::List)
        s_scroll = std::clamp(s_scroll + (MouseWheel > 0 ? -1 : 1), 0, MaxScroll(state));

    if (IsPress(VK_LBUTTON))
    {
        Draw::SwallowHeldClick();
        if (s_mode == Mode::Form)
            FormClick();
        else
            ListClick(state);
    }

    // The window swallows the mouse: a click on it must not walk the hero.
    return false;
}

bool CMobaRoomBrowser::UpdateKeyEvent()
{
    const RoomList::State& state = RoomList::Get();
    if (!state.open || Room::Get().present)
        return true;

    if (IsPress(VK_ESCAPE))
    {
        if (s_mode == Mode::Form)
        {
            s_mode = Mode::List;
            CUITextInputBox::ReleaseFocus();
        }
        else
        {
            CloseBrowser();
        }

        return false;
    }

    if (s_mode == Mode::Form && IsPress(VK_RETURN))
    {
        SubmitForm();
        return false;
    }

    return true;
}

float CMobaRoomBrowser::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaRoomBrowser::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
