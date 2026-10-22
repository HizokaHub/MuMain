#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaMenuPanel.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "Core/Utilities/UsefulDef.h"         // DivideStringByPixel
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaMenu.h"

#include <algorithm>
#include <cmath>
#include <vector>

extern double WorldTime;

using namespace SEASON3B;
namespace Menu = World::Moba::Menu;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen.
    constexpr int PANEL_W = 440;
    constexpr int PAD = 16;
    constexpr int PANEL_Y_TOP = 30;
    constexpr int TITLE_Y = 12;
    constexpr int TEXT_Y = 50;
    constexpr int LINE_H = 16;
    constexpr int MAX_TEXT_LINES = 10;
    constexpr int LINE_COLUMNS = 128;
    constexpr int OPTIONS_GAP = 12;
    constexpr int BUTTON_H = 26;
    constexpr int BUTTON_GAP = 6;
    constexpr int ONE_COLUMN_UP_TO = 6;
    constexpr int BOTTOM_PAD = 16;
    constexpr int CLOSE_SIZE = 20;
    constexpr int TEXT_DY = 3;

    constexpr int CODE_BOX_W = 200;
    constexpr int CODE_BOX_H = 40;
    constexpr int CODE_BUTTON_W = 130;
    constexpr double CARET_BLINK_HZ = 1.5;
    constexpr double MS_PER_SECOND = 1000.0;

    constexpr float LAYER_DEPTH = 10.7f;
    constexpr float KEY_EVENT_ORDER = 100.0f;
    constexpr DWORD UI_KEY_OFFSET = 6;

    // The text of the menu cut into lines that fit the panel (the font has to be set by the caller).
    struct TextLines
    {
        wchar_t rows[MAX_TEXT_LINES][LINE_COLUMNS] = {};
        int count = 0;
    };

    TextLines WrapText(const std::wstring& text)
    {
        TextLines lines;
        size_t start = 0;
        while (start <= text.size() && lines.count < MAX_TEXT_LINES)
        {
            const size_t end = std::min(text.find(L'\n', start), text.size());
            const std::wstring paragraph = text.substr(start, end - start);
            if (!paragraph.empty())
                lines.count += ::DivideStringByPixel(&lines.rows[lines.count][0], MAX_TEXT_LINES - lines.count, LINE_COLUMNS, paragraph.c_str(), PANEL_W - 2 * PAD, false);
            start = end + 1;
        }

        lines.count = std::min(lines.count, MAX_TEXT_LINES);
        return lines;
    }

    int ColumnsOf(const Menu::State& state)
    {
        return static_cast<int>(state.options.size()) > ONE_COLUMN_UP_TO ? 2 : 1;
    }

    int OptionRows(const Menu::State& state)
    {
        const int columns = ColumnsOf(state);
        return (static_cast<int>(state.options.size()) + columns - 1) / columns;
    }

    int PanelHeight(const Menu::State& state, int textLines)
    {
        const int body = state.mode == Menu::Mode::Code
            ? 40 + CODE_BOX_H + OPTIONS_GAP + BUTTON_H
            : textLines * LINE_H + OPTIONS_GAP + OptionRows(state) * (BUTTON_H + BUTTON_GAP);
        return TEXT_Y + body + BOTTOM_PAD;
    }

    Rect Panel(const Menu::State& state, int textLines)
    {
        return { (REFERENCE_WIDTH - PANEL_W) / 2, PANEL_Y_TOP, PANEL_W, PanelHeight(state, textLines) };
    }

    Rect CloseButton(const Rect& panel) { return { panel.x + panel.w - CLOSE_SIZE - 6, panel.y + 6, CLOSE_SIZE, CLOSE_SIZE }; }

    // The rectangle of an option button.
    Rect OptionRect(const Menu::State& state, const Rect& panel, int textLines, int index)
    {
        const int columns = ColumnsOf(state);
        const int width = (panel.w - 2 * PAD - (columns - 1) * BUTTON_GAP) / columns;
        const int top = panel.y + TEXT_Y + textLines * LINE_H + OPTIONS_GAP;
        return { panel.x + PAD + (index % columns) * (width + BUTTON_GAP), top + (index / columns) * (BUTTON_H + BUTTON_GAP), width, BUTTON_H };
    }

    Rect CodeBox(const Rect& panel) { return { panel.x + (panel.w - CODE_BOX_W) / 2, panel.y + TEXT_Y + 34, CODE_BOX_W, CODE_BOX_H }; }

    Rect CodeButton(const Rect& panel, bool submit)
    {
        const int gap = 12;
        const int left = panel.x + (panel.w - (2 * CODE_BUTTON_W + gap)) / 2;
        return { left + (submit ? 0 : CODE_BUTTON_W + gap), CodeBox(panel).y + CODE_BOX_H + OPTIONS_GAP, CODE_BUTTON_W, BUTTON_H };
    }

    // The option under the mouse, or -1.
    int HoveredOption(const Menu::State& state, const Rect& panel, int textLines)
    {
        for (int i = 0; i < static_cast<int>(state.options.size()); ++i)
        {
            if (Draw::MouseInRect(OptionRect(state, panel, textLines, i)))
                return i;
        }

        return -1;
    }

    void RenderCloseButton(const Rect& panel)
    {
        const Rect close = CloseButton(panel);
        Draw::FillRect(close, 0.30f, 0.12f, 0.12f, 1.f);
        Draw::FrameRect(close, 0.95f, 0.80f, 0.40f, Draw::MouseInRect(close) ? 1.f : 0.6f);
        EndRenderColor();
        Draw::PutTextCentered(Draw::Bold(), close.x, close.y + 1, close.w, L"X", 255, 244, 214);
    }

    void RenderOptions(const Menu::State& state, const Rect& panel, const TextLines& lines)
    {
        for (int i = 0; i < lines.count; ++i)
            Draw::PutTextLeft(Draw::Body(), panel.x + PAD, panel.y + TEXT_Y + i * LINE_H + TEXT_DY, panel.w - 2 * PAD, lines.rows[i], 233, 228, 212);

        for (int i = 0; i < static_cast<int>(state.options.size()); ++i)
            Draw::Button(OptionRect(state, panel, lines.count, i), state.options[i].c_str(), true);
    }

    void RenderCodeBox(const Menu::State& state, const Rect& panel)
    {
        Draw::PutTextCentered(Draw::Body(), panel.x, panel.y + TEXT_Y + TEXT_DY, panel.w, L"Escribe el código de la sala y pulsa Intro", 233, 228, 212);

        const Rect box = CodeBox(panel);
        Draw::FillRect(box, 0.02f, 0.03f, 0.05f, 1.f);
        Draw::FrameRect(box, 0.78f, 0.64f, 0.29f, 1.f);
        EndRenderColor();

        const bool caretOn = std::fmod(WorldTime / MS_PER_SECOND * CARET_BLINK_HZ, 1.0) < 0.5;
        const std::wstring shown = state.code + ((caretOn && static_cast<int>(state.code.size()) < Menu::MAX_CODE_LENGTH) ? L"_" : L"");
        Draw::PutTextCentered(Draw::Title(), box.x, box.y + 4, box.w, shown.c_str(), 241, 196, 83);

        Draw::Button(CodeButton(panel, true), L"UNIRME", !state.code.empty());
        Draw::Button(CodeButton(panel, false), L"VOLVER", false);
    }

    // The code box has no text field: the keys are read one by one.
    bool TypeCodeKeys()
    {
        bool used = false;
        for (int key = 'A'; key <= 'Z'; ++key)
        {
            if (IsPress(key))
            {
                Menu::TypeCode(static_cast<wchar_t>(key));
                used = true;
            }
        }

        for (int key = '0'; key <= '9'; ++key)
        {
            if (IsPress(key))
            {
                Menu::TypeCode(static_cast<wchar_t>(key));
                used = true;
            }
        }

        if (IsPress(VK_BACK))
        {
            Menu::EraseCode();
            used = true;
        }

        return used;
    }
}

void CMobaMenuPanel::Create(CNewUIManager* pNewUIMng)
{
    static CMobaMenuPanel s_panel;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_panel);
}

bool CMobaMenuPanel::Render()
{
    const Menu::State& state = Menu::Get();
    if (state.mode == Menu::Mode::Hidden)
        return true;

    const DWORD backColor = g_pRenderText->GetTextColor();
    g_pRenderText->SetFont(Draw::Body());
    const TextLines lines = WrapText(state.text);
    const Rect panel = Panel(state, state.mode == Menu::Mode::Code ? 0 : lines.count);

    Draw::FillRect(panel, 0.04f, 0.055f, 0.086f, 1.f);
    Draw::FrameRect(panel, 0.78f, 0.64f, 0.29f, 1.f);
    EndRenderColor();
    RenderCloseButton(panel);
    Draw::PutTextCentered(Draw::Title(), panel.x, panel.y + TITLE_Y - 4, panel.w, state.title.c_str(), 241, 196, 83);

    if (state.mode == Menu::Mode::Code)
        RenderCodeBox(state, panel);
    else
        RenderOptions(state, panel, lines);

    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaMenuPanel::Update()
{
    return true;
}

bool CMobaMenuPanel::UpdateMouseEvent()
{
    if (Draw::HeldClickSwallowed())
        return false;

    const Menu::State& state = Menu::Get();
    if (state.mode == Menu::Mode::Hidden)
        return true;

    g_pRenderText->SetFont(Draw::Body());
    const TextLines lines = WrapText(state.text);
    const int textLines = state.mode == Menu::Mode::Code ? 0 : lines.count;
    const Rect panel = Panel(state, textLines);
    if (!Draw::MouseInRect(panel))
        return true;

    if (!IsPress(VK_LBUTTON))
        return false;

    Draw::SwallowHeldClick();
    if (Draw::MouseInRect(CloseButton(panel)))
    {
        Menu::CloseByPlayer();
    }
    else if (state.mode == Menu::Mode::Code)
    {
        if (Draw::MouseInRect(CodeButton(panel, true)))
            Menu::SubmitCode();
        else if (Draw::MouseInRect(CodeButton(panel, false)))
            Menu::BackToOptions();
    }
    else
    {
        Menu::Choose(HoveredOption(state, panel, textLines));
    }

    // The panel swallows the mouse: a click on it must not walk the hero.
    return false;
}

bool CMobaMenuPanel::UpdateKeyEvent()
{
    const Menu::State& state = Menu::Get();
    if (state.mode == Menu::Mode::Hidden)
        return true;

    if (IsPress(VK_ESCAPE))
    {
        if (state.mode == Menu::Mode::Code)
            Menu::BackToOptions();
        else
            Menu::CloseByPlayer();
        return false;
    }

    if (state.mode != Menu::Mode::Code)
        return true;

    if (IsPress(VK_RETURN))
    {
        Menu::SubmitCode();
        return false;
    }

    // While the box is open every key belongs to it (the letters would otherwise trigger the skill hotkeys).
    TypeCodeKeys();
    return false;
}

float CMobaMenuPanel::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaMenuPanel::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
