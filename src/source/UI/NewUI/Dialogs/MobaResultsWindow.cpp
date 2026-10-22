#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaResultsWindow.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "App/Platform/Windows/Winmain.h"     // g_hFont, g_hFontBold, g_hFontBig
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaResults.h"

#include <algorithm>
#include <cmath>

using namespace SEASON3B;
namespace Results = World::Moba::Results;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen.
    constexpr int PANEL_W = 560;
    constexpr int PANEL_H = 336;
    constexpr int PANEL_X = (REFERENCE_WIDTH - PANEL_W) / 2;
    constexpr int PANEL_Y = 60;

    constexpr int TITLE_Y = 10;
    constexpr int SUBTITLE_Y = 38;
    constexpr int HEADER_Y = 60;
    constexpr int ROWS_Y = 78;
    constexpr int ROW_H = 16;
    constexpr int TEXT_DY = 2;
    constexpr int BUTTON_W = 220;
    constexpr int BUTTON_H = 26;
    constexpr int BUTTON_BOTTOM_MARGIN = 38;
    constexpr int HINT_BOTTOM_MARGIN = 14;

    // Table columns: left edge from the panel and width.
    struct Column
    {
        int x;
        int w;
        const wchar_t* title;
    };

    constexpr Column COL_NAME = { 16, 126, L"Campeón" };
    constexpr Column COL_CLASS = { 144, 34, L"Clase" };
    constexpr Column COL_LEVEL = { 180, 28, L"Nv" };
    constexpr Column COL_KDA = { 210, 74, L"K / D / A" };
    constexpr Column COL_CREEPS = { 286, 40, L"Minions" };
    constexpr Column COL_GOLD = { 328, 60, L"Oro" };
    constexpr Column COL_DAMAGE = { 392, 76, L"Daño" };
    constexpr Column COL_TAKEN = { 470, 76, L"Recibido" };

    constexpr float LAYER_DEPTH = 10.9f;       // above the ready-check popup (10.8)
    constexpr float KEY_EVENT_ORDER = 100.0f;
    constexpr DWORD UI_KEY_OFFSET = 3;

    constexpr float MS_PER_SECOND = 1000.0f;
    constexpr unsigned int SHORT_NUMBER_FROM = 10000;
    constexpr float THOUSAND = 1000.0f;
    constexpr int SECONDS_PER_MINUTE = 60;
    constexpr int CLASS_COUNT = 7;
    const wchar_t* const CLASS_NAMES[CLASS_COUNT] = { L"DW", L"DK", L"Elf", L"MG", L"DL", L"Inv", L"RF" };

    Rect Panel() { return { PANEL_X, PANEL_Y, PANEL_W, PANEL_H }; }

    Rect ReturnButton()
    {
        return { PANEL_X + (PANEL_W - BUTTON_W) / 2, PANEL_Y + PANEL_H - BUTTON_BOTTOM_MARGIN - BUTTON_H, BUTTON_W, BUTTON_H };
    }

    // "1234" stays, "12345" becomes "12.3k".
    void FormatShort(unsigned int value, wchar_t* out, size_t size)
    {
        if (value < SHORT_NUMBER_FROM)
            swprintf(out, size, L"%u", value);
        else
            swprintf(out, size, L"%.1fk", value / THOUSAND);
    }

    void PutCell(const Column& column, int y, const wchar_t* text, BYTE red, BYTE green, BYTE blue)
    {
        Draw::PutTextCentered(g_hFont, PANEL_X + column.x, y + TEXT_DY, column.w, text, red, green, blue);
    }

    void RenderHeaderTexts()
    {
        const int y = PANEL_Y + HEADER_Y;
        Draw::PutTextLeft(g_hFontBold, PANEL_X + COL_NAME.x, y + TEXT_DY, COL_NAME.w, COL_NAME.title, 170, 179, 194);
        for (const Column* column : { &COL_CLASS, &COL_LEVEL, &COL_KDA, &COL_CREEPS, &COL_GOLD, &COL_DAMAGE, &COL_TAKEN })
            Draw::PutTextCentered(g_hFontBold, PANEL_X + column->x, y + TEXT_DY, column->w, column->title, 170, 179, 194);
    }

    void RenderTitle(const Results::State& state)
    {
        const bool blueWon = state.winner == Results::TEAM_BLUE;
        const bool hasOwnRow = std::any_of(state.rows.begin(), state.rows.end(), [](const Results::Row& row) { return row.mine; });
        const wchar_t* title = !hasOwnRow ? (blueWon ? L"GANA EL EQUIPO AZUL" : L"GANA EL EQUIPO ROJO")
            : Results::IsVictory() ? L"VICTORIA" : L"DERROTA";
        const bool good = !hasOwnRow || Results::IsVictory();
        Draw::PutTextCentered(g_hFontBig, PANEL_X, PANEL_Y + TITLE_Y, PANEL_W, title, good ? 111 : 220, good ? 211 : 90, good ? 154 : 80);

        wchar_t line[96] = {};
        const int minutes = static_cast<int>(state.durationSeconds) / SECONDS_PER_MINUTE;
        const int seconds = static_cast<int>(state.durationSeconds) % SECONDS_PER_MINUTE;
        swprintf(line, std::size(line), L"Nexo %ls destruido  -  duración %d:%02d", blueWon ? L"rojo" : L"azul", minutes, seconds);
        Draw::PutTextCentered(g_hFont, PANEL_X, PANEL_Y + SUBTITLE_Y, PANEL_W, line, 233, 228, 212);
    }

    void RenderRowTexts(const Results::Row& row, int y)
    {
        const BYTE tint = row.bot ? 150 : 233;
        wchar_t cell[32] = {};
        std::wstring name = row.name;
        if (row.bot)
            name += L" (bot)";
        Draw::PutTextLeft(row.mine ? g_hFontBold : g_hFont, PANEL_X + COL_NAME.x, y + TEXT_DY, COL_NAME.w, name.c_str(), tint, tint, row.mine ? 140 : tint);

        PutCell(COL_CLASS, y, row.baseClass < CLASS_COUNT ? CLASS_NAMES[row.baseClass] : L"?", tint, tint, tint);
        swprintf(cell, std::size(cell), L"%d", row.level);
        PutCell(COL_LEVEL, y, cell, tint, tint, tint);
        swprintf(cell, std::size(cell), L"%d / %d / %d", row.kills, row.deaths, row.assists);
        PutCell(COL_KDA, y, cell, tint, tint, tint);
        swprintf(cell, std::size(cell), L"%d", row.creeps);
        PutCell(COL_CREEPS, y, cell, tint, tint, tint);
        FormatShort(row.gold, cell, std::size(cell));
        PutCell(COL_GOLD, y, cell, 241, 196, 83);
        FormatShort(row.damageDealt, cell, std::size(cell));
        PutCell(COL_DAMAGE, y, cell, tint, tint, tint);
        FormatShort(row.damageTaken, cell, std::size(cell));
        PutCell(COL_TAKEN, y, cell, tint, tint, tint);
    }

    // Draws the lines of one team under its label; returns the y after the last line.
    int RenderTeam(const Results::State& state, unsigned char team, int y, bool backgrounds)
    {
        const bool blue = team == Results::TEAM_BLUE;
        if (!backgrounds)
        {
            Draw::PutTextLeft(g_hFontBold, PANEL_X + COL_NAME.x, y + TEXT_DY, COL_NAME.w, blue ? L"EQUIPO AZUL" : L"EQUIPO ROJO", blue ? 110 : 235, blue ? 170 : 100, blue ? 250 : 100);
        }
        else
        {
            Draw::FillRect({ PANEL_X + 8, y, PANEL_W - 16, ROW_H }, blue ? 0.10f : 0.22f, blue ? 0.17f : 0.09f, blue ? 0.32f : 0.10f, 0.9f);
        }

        y += ROW_H;
        for (const Results::Row& row : state.rows)
        {
            if (row.team != team)
                continue;

            if (backgrounds && row.mine)
                Draw::FillRect({ PANEL_X + 8, y, PANEL_W - 16, ROW_H }, 0.35f, 0.30f, 0.10f, 0.85f);
            if (!backgrounds)
                RenderRowTexts(row, y);

            y += ROW_H;
        }

        return y;
    }

    void RenderTable(const Results::State& state, bool backgrounds)
    {
        int y = PANEL_Y + ROWS_Y;
        y = RenderTeam(state, Results::TEAM_BLUE, y, backgrounds);
        RenderTeam(state, Results::TEAM_RED, y, backgrounds);
    }

    void RenderFooterTexts()
    {
        wchar_t line[96] = {};
        const int seconds = static_cast<int>(std::ceil(Results::RemainingMs() / MS_PER_SECOND));
        swprintf(line, std::size(line), L"Volverás a Lorencia automáticamente en %d s", seconds);
        Draw::PutTextCentered(g_hFont, PANEL_X, PANEL_Y + PANEL_H - HINT_BOTTOM_MARGIN - ROW_H + TEXT_DY, PANEL_W, line, 127, 138, 160);
    }
}

void CMobaResultsWindow::Create(CNewUIManager* pNewUIMng)
{
    static CMobaResultsWindow s_window;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_window);
}

bool CMobaResultsWindow::Render()
{
    const Results::State& state = Results::Get();
    if (!state.visible)
        return true;

    const DWORD backColor = g_pRenderText->GetTextColor();
    Draw::FillRect(Panel(), 0.04f, 0.055f, 0.086f, 0.94f);
    Draw::FrameRect(Panel(), 0.78f, 0.64f, 0.29f, 1.f);
    Draw::FillRect({ PANEL_X + 8, PANEL_Y + HEADER_Y, PANEL_W - 16, ROW_H }, 0.09f, 0.11f, 0.16f, 0.95f);
    RenderTable(state, true);
    EndRenderColor();

    Draw::Button(ReturnButton(), L"VOLVER A LORENCIA", true);
    RenderTitle(state);
    RenderHeaderTexts();
    RenderTable(state, false);
    RenderFooterTexts();
    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaResultsWindow::Update()
{
    return true;
}

bool CMobaResultsWindow::UpdateMouseEvent()
{
    if (!Results::Get().visible || !Draw::MouseInRect(Panel()))
        return true;

    if (IsPress(VK_LBUTTON) && Draw::MouseInRect(ReturnButton()))
        Results::ReturnToTown();

    // The panel swallows the mouse: a click on it must not walk the hero.
    return false;
}

bool CMobaResultsWindow::UpdateKeyEvent()
{
    return true;
}

float CMobaResultsWindow::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaResultsWindow::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
