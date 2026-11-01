#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaCommanderWindow.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "Core/Input/KeyState.h"              // IsKeyDown (Shift)
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaCommander.h"

#include <algorithm>
#include <chrono>

using namespace SEASON3B;
namespace Commander = World::Moba::Commander;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen. The essence bar hangs from the top edge; the console sits above the HUD bar.
    constexpr int TOP_W = 300;
    constexpr int TOP_X = (640 - TOP_W) / 2;
    constexpr int TOP_Y = 2;
    constexpr int TOP_H = 28;
    constexpr int BAR_H = 4;
    constexpr int BAR_MARGIN = 6;
    constexpr int ESSENCE_CAP = 3000;          // the server's cap (MobaCommanderBoard); the bar spans 0..cap

    constexpr int CON_W = 340;
    constexpr int CON_X = (640 - CON_W) / 2;
    constexpr int CON_Y = 332;
    constexpr int CON_H = 94;
    constexpr int CON_MARGIN = 8;
    constexpr int CON_CONTENT_W = CON_W - 2 * CON_MARGIN;

    constexpr int LANE_ROW_Y = 6;
    constexpr int LANE_H = 26;
    constexpr int LANE_GAP = 6;
    constexpr int PACK_ROW_Y = 38;
    constexpr int PACK_H = 34;
    constexpr int PACK_GAP = 4;
    constexpr int PACK_BUTTONS = 4;            // up to three packs and the income upgrade
    constexpr int HINT_ROW_Y = 77;
    constexpr int HINT_H = 14;
    constexpr int RELEASE_W = 96;

    constexpr int CLAIM_W = 110;
    constexpr int CLAIM_H = 20;
    constexpr int CLAIM_Y = TOP_Y + TOP_H + 4;

    constexpr int SHIFT_BURST = 5;             // Shift sends a pack this many times
    constexpr double FLASH_MS = 180.0;
    constexpr double SENT_NOTE_MS = 2500.0;
    constexpr double ESSENCE_EASE_PER_SECOND = 8.0;

    constexpr float LAYER_DEPTH = 10.6f;       // below the popups (ready check 10.8) but above the HUD
    constexpr float KEY_EVENT_ORDER = 3.5f;    // before the main frame (2.9) and the hotkeys (1.0): the commander's keys win
    constexpr DWORD UI_KEY_OFFSET = 15;

    constexpr float HOT_SHADE = 1.35f;
    constexpr float IDLE_SHADE = 1.0f;

    const wchar_t* const UNIT_NAMES[] = { L"Soldados", L"Brutos", L"\u00c9lite" };
    const wchar_t* const LANE_NAMES[] = { L"TOP", L"MID", L"BOT" };
    const wchar_t LANE_KEYS[] = { L'1', L'2', L'3' };
    const wchar_t PACK_KEYS[] = { L'Q', L'W', L'E', L'R' };   // R is the income upgrade (the fourth button)
    constexpr int INCOME_BUTTON = 3;

    double s_flashUntil[PACK_BUTTONS] = {};
    double s_noteUntil = 0.0;
    wchar_t s_note[64] = {};
    double s_shownEssence = 0.0;
    double s_lastFrameMs = 0.0;

    double NowMs()
    {
        using namespace std::chrono;
        return static_cast<double>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }

    const wchar_t* UnitName(int unit)
    {
        return unit < static_cast<int>(std::size(UNIT_NAMES)) ? UNIT_NAMES[unit] : L"Paquete";
    }

    Rect TopBar() { return { TOP_X, TOP_Y, TOP_W, TOP_H }; }

    Rect Console() { return { CON_X, CON_Y, CON_W, CON_H }; }

    Rect ClaimButton() { return { (640 - CLAIM_W) / 2, CLAIM_Y, CLAIM_W, CLAIM_H }; }

    Rect LaneCard(int lane)
    {
        const int width = (CON_CONTENT_W - (Commander::LANE_COUNT - 1) * LANE_GAP) / Commander::LANE_COUNT;
        return { CON_X + CON_MARGIN + lane * (width + LANE_GAP), CON_Y + LANE_ROW_Y, width, LANE_H };
    }

    Rect PackButton(int index)
    {
        const int width = (CON_CONTENT_W - (PACK_BUTTONS - 1) * PACK_GAP) / PACK_BUTTONS;
        return { CON_X + CON_MARGIN + index * (width + PACK_GAP), CON_Y + PACK_ROW_Y, width, PACK_H };
    }

    Rect ReleaseButton() { return { CON_X + CON_W - CON_MARGIN - RELEASE_W, CON_Y + HINT_ROW_Y, RELEASE_W, HINT_H }; }

    bool CanBuyIncome(const Commander::State& state)
    {
        return state.nextIncomeCost > 0 && state.essence >= state.nextIncomeCost;
    }

    bool CanAfford(const Commander::State& state, int unit)
    {
        return unit < state.unitCount && state.essence >= state.unitCost[unit];
    }

    // The lane the pack keys use must be one that exists in this format (1v1 and ARAM close the side lanes).
    void EnsureOpenActiveLane(const Commander::State& state)
    {
        if (Commander::IsLaneOpen(state, Commander::ActiveLane()))
            return;

        for (int lane = 0; lane < Commander::LANE_COUNT; ++lane)
        {
            if (Commander::IsLaneOpen(state, lane))
            {
                Commander::SetActiveLane(lane);
                return;
            }
        }
    }

    void Note(const wchar_t* text)
    {
        wcsncpy_s(s_note, text, _TRUNCATE);
        s_noteUntil = NowMs() + SENT_NOTE_MS;
    }

    // Sends a pack down the active lane (Shift = a burst); each order is paid separately, so a burst stops when the essence runs out.
    void SendPack(const Commander::State& state, int unit, bool burst)
    {
        const int lane = Commander::ActiveLane();
        if (unit >= state.unitCount || !Commander::IsLaneOpen(state, lane) || !CanAfford(state, unit))
            return;

        const int times = burst ? std::min(SHIFT_BURST, state.essence / std::max(1, state.unitCost[unit])) : 1;
        for (int i = 0; i < times; ++i)
            Commander::Order(Commander::Action::Send, unit, lane);

        s_flashUntil[unit] = NowMs() + FLASH_MS;
        swprintf(s_note, std::size(s_note), L"Enviado: %ls x%d  ->  %ls", UnitName(unit), times, LANE_NAMES[lane]);
        s_noteUntil = NowMs() + SENT_NOTE_MS;
    }

    void BuyIncome(const Commander::State& state)
    {
        if (!CanBuyIncome(state))
            return;

        Commander::Order(Commander::Action::Income);
        s_flashUntil[INCOME_BUTTON] = NowMs() + FLASH_MS;
        Note(L"Ingreso mejorado");
    }

    // The essence shown eases toward the real value so the bar moves smoothly between the one-second updates.
    void EaseEssence(const Commander::State& state)
    {
        const double now = NowMs();
        const double dt = s_lastFrameMs > 0.0 ? std::min(0.25, (now - s_lastFrameMs) / 1000.0) : 1.0;
        s_lastFrameMs = now;
        const double k = std::min(1.0, dt * ESSENCE_EASE_PER_SECOND);
        s_shownEssence += (static_cast<double>(state.essence) - s_shownEssence) * k;
        if (std::abs(s_shownEssence - state.essence) < 0.5)
            s_shownEssence = state.essence;
    }

    void RenderTopBar(const Commander::State& state)
    {
        const Rect bar = TopBar();
        Draw::FillRect(bar, 0.04f, 0.055f, 0.086f, 0.86f);
        Draw::FrameRect(bar, 0.78f, 0.64f, 0.29f, 1.f);

        // The essence bar, with a tick at the cost of each pack so you can see what you can afford.
        const Rect track = { bar.x + BAR_MARGIN, bar.y + bar.h - BAR_H - 3, bar.w - 2 * BAR_MARGIN, BAR_H };
        Draw::FillRect(track, 0.10f, 0.13f, 0.20f, 1.f);
        const int filled = static_cast<int>(track.w * std::clamp(s_shownEssence / ESSENCE_CAP, 0.0, 1.0));
        if (filled > 0)
            Draw::FillRect({ track.x, track.y, filled, track.h }, 0.31f, 0.71f, 1.0f, 1.f);
        for (int unit = 0; unit < state.unitCount; ++unit)
        {
            const int tickX = track.x + track.w * std::min(state.unitCost[unit], ESSENCE_CAP) / ESSENCE_CAP;
            Draw::FillRect({ tickX, track.y - 2, 1, track.h + 4 }, 0.95f, 0.78f, 0.33f, 1.f);
        }

        EndRenderColor();

        wchar_t line[96] = {};
        swprintf(line, std::size(line), L"%d", static_cast<int>(s_shownEssence + 0.5));
        Draw::PutTextLeft(Draw::Bold(), bar.x + 10, bar.y + 4, 80, line, 127, 212, 255);
        swprintf(line, std::size(line), L"+%d/s   ingreso %d/%d", state.incomePerSecond, state.incomeLevel, state.maxIncomeLevel);
        Draw::PutTextCentered(Draw::Body(), bar.x + 70, bar.y + 4, bar.w - 80, line, 190, 198, 212);
    }

    void RenderLaneCard(const Commander::State& state, int lane)
    {
        const Rect r = LaneCard(lane);
        const bool open = Commander::IsLaneOpen(state, lane);
        const bool active = lane == Commander::ActiveLane();
        const bool pressed = open && Commander::LaneUnderPressure(state, lane);
        const bool hot = open && Draw::MouseInRect(r);

        float red = 0.09f, green = 0.12f, blue = 0.19f;
        float frameR = 0.23f, frameG = 0.27f, frameB = 0.35f;
        if (pressed)
        {
            red = 0.27f; green = 0.20f; blue = 0.07f;
            frameR = 0.96f; frameG = 0.75f; frameB = 0.29f;
        }
        if (active)
        {
            blue += 0.14f; green += 0.05f;
            frameR = 0.31f; frameG = 0.71f; frameB = 1.0f;
        }
        const float shade = hot ? HOT_SHADE : IDLE_SHADE;
        Draw::FillRect(r, red * shade, green * shade, blue * shade, 0.95f);
        Draw::FrameRect(r, open ? frameR : 0.18f, open ? frameG : 0.2f, open ? frameB : 0.25f, 1.f);
        EndRenderColor();

        const int textY = r.y + (r.h - Draw::BodyLineHeight()) / 2;
        wchar_t label[24] = {};
        swprintf(label, std::size(label), L"%lc %ls", LANE_KEYS[lane], LANE_NAMES[lane]);
        if (!open)
        {
            Draw::PutTextLeft(Draw::Bold(), r.x + 6, textY, 40, label, 90, 98, 115);
            return;
        }

        Draw::PutTextLeft(Draw::Bold(), r.x + 6, textY, 40, label, active ? 255 : 220, active ? 244 : 224, active ? 214 : 200);
        wchar_t counts[32] = {};
        swprintf(counts, std::size(counts), L"%d vs %d", state.ownCreeps[lane], state.enemyCreeps[lane]);
        if (pressed)
            Draw::PutTextCentered(Draw::Body(), r.x + 44, textY, r.w - 48, counts, 255, 200, 90);
        else
            Draw::PutTextCentered(Draw::Body(), r.x + 44, textY, r.w - 48, counts, 190, 198, 212);
    }

    void RenderPackButton(const Commander::State& state, int index)
    {
        const bool income = index == INCOME_BUTTON;
        if (!income && index >= state.unitCount)
            return;

        const Rect r = PackButton(index);
        const bool enabled = income
            ? CanBuyIncome(state)
            : CanAfford(state, index) && Commander::IsLaneOpen(state, Commander::ActiveLane());
        const bool hot = enabled && Draw::MouseInRect(r);
        const bool flash = NowMs() < s_flashUntil[index];
        const float shade = flash ? 1.9f : (hot ? HOT_SHADE : IDLE_SHADE);
        if (enabled)
            Draw::FillRect(r, 0.10f * shade, 0.20f * shade, 0.34f * shade, 0.95f);
        else
            Draw::FillRect(r, 0.09f, 0.11f, 0.16f, 0.95f);
        Draw::FrameRect(r, enabled ? 0.31f : 0.23f, enabled ? 0.71f : 0.27f, enabled ? 1.0f : 0.35f, 1.f);
        EndRenderColor();

        wchar_t top[40] = {};
        wchar_t bottom[24] = {};
        if (income)
        {
            swprintf(top, std::size(top), L"%lc Ingreso", PACK_KEYS[index]);
            if (state.nextIncomeCost > 0)
                swprintf(bottom, std::size(bottom), L"%d", state.nextIncomeCost);
            else
                swprintf(bottom, std::size(bottom), L"M\u00e1ximo");
        }
        else
        {
            swprintf(top, std::size(top), L"%lc %ls", PACK_KEYS[index], UnitName(index));
            swprintf(bottom, std::size(bottom), L"%d", state.unitCost[index]);
        }

        const int lineH = Draw::BodyLineHeight();
        if (enabled)
        {
            Draw::PutTextCentered(Draw::Body(), r.x, r.y + 3, r.w, top, 255, 244, 214);
            Draw::PutTextCentered(Draw::Body(), r.x, r.y + 4 + lineH, r.w, bottom, 241, 196, 83);
        }
        else
        {
            Draw::PutTextCentered(Draw::Body(), r.x, r.y + 3, r.w, top, 110, 118, 135);
            Draw::PutTextCentered(Draw::Body(), r.x, r.y + 4 + lineH, r.w, bottom, 110, 118, 135);
        }
    }

    void RenderHintRow()
    {
        const Rect release = ReleaseButton();
        const bool hot = Draw::MouseInRect(release);
        if (NowMs() < s_noteUntil)
            Draw::PutTextLeft(Draw::Body(), CON_X + CON_MARGIN, release.y, CON_CONTENT_W - RELEASE_W, s_note, 127, 212, 255);
        else
            Draw::PutTextLeft(Draw::Body(), CON_X + CON_MARGIN, release.y, CON_CONTENT_W - RELEASE_W, L"May\u00fas = x5   Flechas = mover c\u00e1mara", 140, 150, 168);
        Draw::PutTextCentered(Draw::Body(), release.x, release.y, release.w, L"Soltar el mando", hot ? 255 : 170, hot ? 220 : 179, hot ? 170 : 194);
    }

    void RenderConsole(const Commander::State& state)
    {
        EnsureOpenActiveLane(state);
        Draw::FillRect(Console(), 0.04f, 0.055f, 0.086f, 0.88f);
        Draw::FrameRect(Console(), 0.78f, 0.64f, 0.29f, 1.f);
        EndRenderColor();

        for (int lane = 0; lane < Commander::LANE_COUNT; ++lane)
            RenderLaneCard(state, lane);
        for (int index = 0; index < PACK_BUTTONS; ++index)
            RenderPackButton(state, index);
        RenderHintRow();
    }

    void RenderClaim()
    {
        const Rect r = ClaimButton();
        const bool hot = Draw::MouseInRect(r);
        const float shade = hot ? HOT_SHADE : IDLE_SHADE;
        Draw::FillRect(r, 0.12f * shade, 0.36f * shade, 0.24f * shade, 0.95f);
        Draw::FrameRect(r, 0.78f, 0.64f, 0.29f, 1.f);
        EndRenderColor();
        Draw::PutTextCentered(Draw::Body(), r.x, r.y + (r.h - Draw::BodyLineHeight()) / 2, r.w, L"Tomar el mando", 255, 244, 214);
    }

    bool HandleConsoleClick(const Commander::State& state)
    {
        for (int lane = 0; lane < Commander::LANE_COUNT; ++lane)
        {
            if (Draw::MouseInRect(LaneCard(lane)) && Commander::IsLaneOpen(state, lane))
            {
                Commander::SetActiveLane(lane);
                return true;
            }
        }

        const bool burst = Core::Input::IsKeyDown(VK_SHIFT);
        for (int index = 0; index < PACK_BUTTONS; ++index)
        {
            if (!Draw::MouseInRect(PackButton(index)))
                continue;

            if (index == INCOME_BUTTON)
                BuyIncome(state);
            else
                SendPack(state, index, burst);
            return true;
        }

        if (Draw::MouseInRect(ReleaseButton()))
        {
            Commander::Order(Commander::Action::Release);
            return true;
        }

        return false;
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
    {
        s_lastFrameMs = 0.0;
        return true;
    }

    const DWORD backColor = g_pRenderText->GetTextColor();
    EaseEssence(state);
    RenderTopBar(state);
    if (state.isCommander)
        RenderConsole(state);
    else if (state.commandFree)
        RenderClaim();
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
    if (!state.visible)
        return true;

    const bool overConsole = state.isCommander && Draw::MouseInRect(Console());
    const bool overClaim = !state.isCommander && state.commandFree && Draw::MouseInRect(ClaimButton());
    const bool overTop = Draw::MouseInRect(TopBar());
    if (!overConsole && !overClaim && !overTop)
        return true;

    if (IsPress(VK_LBUTTON))
    {
        Draw::SwallowHeldClick();
        if (overConsole)
            HandleConsoleClick(state);
        else if (overClaim)
            Commander::Order(Commander::Action::Claim);
    }

    // The panel swallows the mouse: a click on it must not walk the hero.
    return false;
}

bool CMobaCommanderWindow::UpdateKeyEvent()
{
    if (!Commander::CommandingNow())
        return true;

    const Commander::State& state = Commander::Get();
    EnsureOpenActiveLane(state);

    // 1/2/3 pick the lane; Q/W/E send a pack down it (Shift = a burst of five); R buys the next income level.
    for (int lane = 0; lane < Commander::LANE_COUNT; ++lane)
    {
        if (IsPress('1' + lane))
        {
            if (Commander::IsLaneOpen(state, lane))
                Commander::SetActiveLane(lane);
            return false;
        }
    }

    const bool burst = Core::Input::IsKeyDown(VK_SHIFT);
    for (int unit = 0; unit < INCOME_BUTTON; ++unit)
    {
        if (IsPress(static_cast<int>(PACK_KEYS[unit])))
        {
            SendPack(state, unit, burst);
            return false;
        }
    }

    if (IsPress(static_cast<int>(PACK_KEYS[INCOME_BUTTON])))
    {
        BuyIncome(state);
        return false;
    }

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
