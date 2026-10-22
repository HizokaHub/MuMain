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
#include <vector>

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

    // The create form and the password prompt.
    constexpr int FORM_LABEL_X = 30;
    constexpr int FORM_FIELD_X = 200;
    constexpr int FORM_NAME_Y = 58;
    constexpr int FORM_PASS_Y = 92;
    constexpr int FORM_SIZE_Y = 130;
    constexpr int FORM_MAP_Y = 172;
    constexpr int FORM_BOTS_Y = 214;
    constexpr int FORM_SUMMARY_Y = 266;
    constexpr int PROMPT_BOX_Y = 120;
    constexpr int BOX_W = 330;
    constexpr int BOX_H = 22;
    constexpr int NAME_MAX_LENGTH = 24;
    constexpr int PASSWORD_MAX_LENGTH = 16;
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

    enum class Mode { List, Form, Password };

    struct FormState
    {
        int teamSize = 3;
        bool midOnly = false;
        bool bots = true;
    };

    Mode s_mode = Mode::List;
    FormState s_form;
    int s_scroll = 0;
    unsigned int s_joinId = 0;
    double s_lastRefreshMs = 0.0;
    bool s_wasOpen = false;
    std::unique_ptr<CUITextInputBox> s_nameBox;
    std::unique_ptr<CUITextInputBox> s_passBox;   // the optional password of a new room
    std::unique_ptr<CUITextInputBox> s_joinBox;   // the password typed to join a locked room (hidden)

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

    Rect BoxFrame(int y) { return { PANEL_X + FORM_FIELD_X, PANEL_Y + y, BOX_W + 12, BOX_H + 8 }; }

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

    std::unique_ptr<CUITextInputBox> MakeBox(int maxLength, bool hidden)
    {
        auto box = std::make_unique<CUITextInputBox>();
        box->Init(g_hWnd, BOX_W, BOX_H, maxLength, hidden ? TRUE : FALSE);
        box->SetTextColor(255, 255, 230, 210);
        box->SetBackColor(0, 0, 0, 0);
        box->SetState(UISTATE_NORMAL);
        box->SetFontProvider(&Draw::Body);
        return box;
    }

    CUITextInputBox& NameBox()
    {
        if (!s_nameBox)
            s_nameBox = MakeBox(NAME_MAX_LENGTH, false);
        return *s_nameBox;
    }

    CUITextInputBox& PassBox()
    {
        if (!s_passBox)
            s_passBox = MakeBox(PASSWORD_MAX_LENGTH, false);
        return *s_passBox;
    }

    CUITextInputBox& JoinBox()
    {
        if (!s_joinBox)
            s_joinBox = MakeBox(PASSWORD_MAX_LENGTH, true);
        return *s_joinBox;
    }

    bool AnyBoxFocused()
    {
        return (s_nameBox && s_nameBox->HaveFocus()) || (s_passBox && s_passBox->HaveFocus()) || (s_joinBox && s_joinBox->HaveFocus());
    }

    HWND FocusedBoxHandle()
    {
        for (const std::unique_ptr<CUITextInputBox>* box : { &s_nameBox, &s_passBox, &s_joinBox })
        {
            if (*box && (*box)->HaveFocus())
                return (*box)->GetHandle();
        }

        return g_hWnd;
    }

    std::wstring TextOf(CUITextInputBox& box, int maxLength)
    {
        std::vector<wchar_t> text(maxLength + 2, L'\0');
        box.GetText(text.data(), maxLength + 1);
        return text.data();
    }

    void OpenForm()
    {
        s_mode = Mode::Form;
        NameBox().SetText(nullptr);
        PassBox().SetText(nullptr);
        NameBox().GiveFocus();
    }

    void OpenPasswordPrompt(unsigned int roomId)
    {
        s_mode = Mode::Password;
        s_joinId = roomId;
        JoinBox().SetText(nullptr);
        JoinBox().GiveFocus();
    }

    void CloseBrowser()
    {
        if (AnyBoxFocused())
            CUITextInputBox::ReleaseFocus();

        s_mode = Mode::List;
        RoomList::Close();
    }

    void SubmitForm()
    {
        RoomList::Create(TextOf(NameBox(), NAME_MAX_LENGTH), s_form.teamSize, s_form.midOnly, s_form.bots, TextOf(PassBox(), PASSWORD_MAX_LENGTH));
        CUITextInputBox::ReleaseFocus();
    }

    void SubmitPassword()
    {
        RoomList::Join(s_joinId, TextOf(JoinBox(), PASSWORD_MAX_LENGTH));
        CUITextInputBox::ReleaseFocus();
        s_mode = Mode::List;
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
        const std::wstring name = (room.locked ? L"[clave] " : L"") + room.name;
        Draw::PutTextLeft(Draw::Bold(), PANEL_X + COL_NAME.x + 2, y, COL_NAME.w, name.c_str(), 241, 196, 83);
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

    void RenderBox(CUITextInputBox& box, const Rect& frame)
    {
        Draw::FillRect(frame, 0.02f, 0.03f, 0.05f, 1.f);
        Draw::FrameRect(frame, box.HaveFocus() ? 0.95f : 0.78f, box.HaveFocus() ? 0.80f : 0.64f, box.HaveFocus() ? 0.40f : 0.29f, 1.f);
        EndRenderColor();
        box.SetPosition(frame.x + 8, frame.y + (frame.h - Draw::BodyLineHeight()) / 2);
        box.Render();
    }

    void RenderFormTexts()
    {
        const int labelWidth = FORM_FIELD_X - FORM_LABEL_X;
        Draw::PutTextLeft(Draw::Bold(), PANEL_X + FORM_LABEL_X, PANEL_Y + FORM_NAME_Y + 6, labelWidth, L"Nombre de la sala", 233, 228, 212);
        Draw::PutTextLeft(Draw::Bold(), PANEL_X + FORM_LABEL_X, PANEL_Y + FORM_PASS_Y + 6, labelWidth, L"Clave (opcional)", 233, 228, 212);
        Draw::PutTextLeft(Draw::Bold(), PANEL_X + FORM_LABEL_X, PANEL_Y + FORM_SIZE_Y + 5, labelWidth, L"Jugadores por equipo", 233, 228, 212);
        Draw::PutTextLeft(Draw::Bold(), PANEL_X + FORM_LABEL_X, PANEL_Y + FORM_MAP_Y + 5, labelWidth, L"Mapa", 233, 228, 212);
        Draw::PutTextLeft(Draw::Bold(), PANEL_X + FORM_LABEL_X, PANEL_Y + FORM_BOTS_Y + 5, labelWidth, L"Bots", 233, 228, 212);

        wchar_t summary[160] = {};
        swprintf(summary, std::size(summary), L"Se jugar\u00e1 %dv%d %ls, %ls.", s_form.teamSize, s_form.teamSize,
            s_form.midOnly ? L"solo por el carril central (sin jungla)" : L"en los 3 carriles con jungla",
            s_form.bots ? L"rellenando con bots los lugares libres" : L"solo con jugadores (los equipos tienen que quedar iguales)");
        Draw::PutTextCentered(Draw::Body(), PANEL_X, PANEL_Y + FORM_SUMMARY_Y, PANEL_W, summary, 170, 179, 194);
    }

    void RenderForm()
    {
        for (int size = 1; size <= MAX_TEAM_SIZE; ++size)
            ChoiceDraw(SizeButton(size), std::to_wstring(size).c_str(), s_form.teamSize == size);

        ChoiceDraw(ChoiceButton(FORM_MAP_Y, 0), L"3 carriles + jungla", !s_form.midOnly);
        ChoiceDraw(ChoiceButton(FORM_MAP_Y, 1), L"Solo carril central", s_form.midOnly);
        ChoiceDraw(ChoiceButton(FORM_BOTS_Y, 0), L"Con bots", s_form.bots);
        ChoiceDraw(ChoiceButton(FORM_BOTS_Y, 1), L"Sin bots", !s_form.bots);

        Draw::Button(FooterButton(0, 2), L"CREAR SALA", true);
        Draw::Button(FooterButton(1, 2), L"VOLVER", false);
        RenderFormTexts();
        RenderBox(NameBox(), BoxFrame(FORM_NAME_Y));
        RenderBox(PassBox(), BoxFrame(FORM_PASS_Y));
    }

    void RenderPrompt()
    {
        Draw::PutTextCentered(Draw::Body(), PANEL_X, PANEL_Y + PROMPT_BOX_Y - 36, PANEL_W, L"Esta sala tiene clave. Escr\u00edbela y pulsa Intro.", 233, 228, 212);
        RenderBox(JoinBox(), BoxFrame(PROMPT_BOX_Y));
        Draw::Button(FooterButton(0, 2), L"ENTRAR", true);
        Draw::Button(FooterButton(1, 2), L"VOLVER", false);
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
            if (!Draw::MouseInRect(JoinButton(row)) || room.players >= 2 * room.teamSize)
                continue;

            if (room.locked)
                OpenPasswordPrompt(room.id);
            else
                RoomList::Join(room.id, std::wstring());
            return;
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

    void PromptClick()
    {
        if (Draw::MouseInRect(FooterButton(0, 2)))
            SubmitPassword();
        else if (Draw::MouseInRect(FooterButton(1, 2)))
            s_mode = Mode::List;
    }

    const wchar_t* TitleOf()
    {
        switch (s_mode)
        {
        case Mode::Form: return L"CREAR SALA";
        case Mode::Password: return L"SALA CON CLAVE";
        default: return L"SALAS PRIVADAS";
        }
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

    Draw::PutTextCentered(Draw::Title(), PANEL_X, PANEL_Y + TITLE_Y, PANEL_W, TitleOf(), 241, 196, 83);
    if (s_mode == Mode::Form)
        RenderForm();
    else if (s_mode == Mode::Password)
        RenderPrompt();
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
        if (s_wasOpen && AnyBoxFocused())
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
    SetRelatedWnd(FocusedBoxHandle());
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
    {
        NameBox().DoAction();
        PassBox().DoAction();
    }
    else if (s_mode == Mode::Password)
    {
        JoinBox().DoAction();
    }

    if (MouseWheel != 0 && s_mode == Mode::List)
        s_scroll = std::clamp(s_scroll + (MouseWheel > 0 ? -1 : 1), 0, MaxScroll(state));

    if (IsPress(VK_LBUTTON))
    {
        Draw::SwallowHeldClick();
        if (s_mode == Mode::Form)
            FormClick();
        else if (s_mode == Mode::Password)
            PromptClick();
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
        if (s_mode != Mode::List)
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

    if (IsPress(VK_RETURN))
    {
        if (s_mode == Mode::Form)
        {
            SubmitForm();
            return false;
        }

        if (s_mode == Mode::Password)
        {
            SubmitPassword();
            return false;
        }
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
