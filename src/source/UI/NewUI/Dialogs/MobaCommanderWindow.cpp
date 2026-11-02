#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaCommanderWindow.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUISystem.h"             // g_pChatListBox, g_pChatInputBox, g_pNewUISystem (chat moved above the console)
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/NewUI/HUD/NewUIMainFrameWindow.h"   // RenderMobaScoreboardOverlay (TAB scoreboard in the RTS view)
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "Render/Terrain/ZzzLodTerrain.h"     // TERRAIN_SCALE
#include "Camera/CameraManager.h"             // FocusMobaCameraOn (F1/F2/F3)
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
    // Layout, in the 640x480 reference screen. The essence bar hangs from the top edge with the wave timer under it; the RTS console
    // (the commander only) is a full-width panel at the bottom: the minimap at its left end (drawn by the minimap itself, which moves
    // there while the RTS view is on), the queue of each lane in the middle and the 3x3 command card at the right.
    constexpr int TOP_W = 300;
    constexpr int TOP_X = (640 - TOP_W) / 2;
    constexpr int TOP_Y = 2;
    constexpr int TOP_H = 28;
    constexpr int BAR_H = 4;
    constexpr int BAR_MARGIN = 6;
    constexpr int ESSENCE_CAP = 3000;          // the server's cap (MobaCommanderBoard); the bar spans 0..cap

    constexpr int TIMER_W = 190;
    constexpr int TIMER_H = 20;
    constexpr int TIMER_Y = TOP_Y + TOP_H + 2;
    constexpr int WARN_SECONDS = 5;            // the timer turns amber for the last seconds before a wave

    constexpr int CON_Y = Commander::RTS_CONSOLE_TOP;
    constexpr int CON_H = 480 - CON_Y;
    constexpr int MAP_W = Commander::RTS_MINIMAP_WIDTH;
    constexpr int CARD_W = 206;                // the command card panel
    constexpr int CARD_X = 640 - CARD_W;
    constexpr int MID_X = MAP_W;               // the queue panel, between the minimap and the card
    constexpr int MID_W = CARD_X - MAP_W;
    constexpr int MID_MARGIN = 6;

    constexpr int LANE_ROW_Y = 5;
    constexpr int LANE_H = 70;
    constexpr int LANE_GAP = 5;
    constexpr int HINT_ROW_Y = 80;
    constexpr int HINT_H = 14;
    constexpr int RELEASE_W = 96;

    constexpr int BTN_COLS = 3;
    constexpr int BTN_ROWS = 3;
    constexpr int BTN_W = 64;
    constexpr int BTN_H = 30;
    constexpr int BTN_GAP_X = 3;
    constexpr int BTN_GAP_Y = 2;
    constexpr int BTN_ORIGIN_X = CARD_X + (CARD_W - (BTN_COLS * BTN_W + (BTN_COLS - 1) * BTN_GAP_X)) / 2;
    constexpr int BTN_ORIGIN_Y = CON_Y + 3;

    constexpr int CLAIM_W = 110;
    constexpr int CLAIM_H = 20;
    constexpr int CLAIM_Y = TIMER_Y + TIMER_H + 4;

    constexpr int SHIFT_BURST = 5;             // Shift sends a pack this many times
    constexpr double FLASH_MS = 180.0;
    constexpr double SENT_NOTE_MS = 2500.0;
    constexpr double ESSENCE_EASE_PER_SECOND = 8.0;

    // The chat sits above the HUD bar (50 px) normally; the console is 100 px tall, so it goes up by the difference while it is on.
    constexpr int CHAT_LOG_Y = 480 - 50 - 47;      // where NewUISystem creates them
    constexpr int CHAT_INPUT_Y = 480 - 51 - 47;
    constexpr int CHAT_RAISE = CON_H - 50;

    constexpr float LAYER_DEPTH = 10.6f;       // below the popups (ready check 10.8) but above the HUD
    constexpr float KEY_EVENT_ORDER = 3.5f;    // before the main frame (2.9) and the hotkeys (1.0): the commander's keys win
    constexpr DWORD UI_KEY_OFFSET = 15;

    constexpr float HOT_SHADE = 1.35f;
    constexpr float IDLE_SHADE = 1.0f;

    const wchar_t* const UNIT_NAMES[] = { L"Soldados", L"Brutos", L"\u00c9lite" };
    const wchar_t* const LANE_NAMES[] = { L"TOP", L"MID", L"BOT" };
    const wchar_t LANE_KEYS[] = { L'1', L'2', L'3' };

    // The 3x3 command card, row by row. Reserved slots are drawn dimmed with "Pronto" and do nothing yet.
    enum class Command
    {
        Unit0, Unit1, Unit2,
        Income, Reserved, Reserved2,
        Reserved3, SendNow, Reserved4,
    };
    constexpr Command COMMANDS[BTN_ROWS * BTN_COLS] =
    {
        Command::Unit0,    Command::Unit1,    Command::Unit2,
        Command::Income,   Command::Reserved, Command::Reserved2,
        Command::Reserved3, Command::SendNow, Command::Reserved4,
    };
    const wchar_t COMMAND_KEYS[BTN_ROWS * BTN_COLS] = { L'Q', L'W', L'E', L'A', L'S', L'D', L'Z', L'X', L'C' };
    constexpr int BUTTON_COUNT = BTN_ROWS * BTN_COLS;

    double s_flashUntil[BUTTON_COUNT] = {};
    double s_noteUntil = 0.0;
    wchar_t s_note[64] = {};
    double s_shownEssence = 0.0;
    double s_lastFrameMs = 0.0;
    bool s_rtsShown = false;

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

    Rect TimerBox() { return { (640 - TIMER_W) / 2, TIMER_Y, TIMER_W, TIMER_H }; }

    Rect Console() { return { 0, CON_Y, 640, CON_H }; }

    // The left end of the console is the minimap's: the console leaves its mouse to it.
    Rect MinimapBox() { return { 0, CON_Y, MAP_W, CON_H }; }

    Rect QueuePanel() { return { MID_X, CON_Y, MID_W, CON_H }; }

    Rect CardPanel() { return { CARD_X, CON_Y, CARD_W, CON_H }; }

    Rect ClaimButton() { return { (640 - CLAIM_W) / 2, CLAIM_Y, CLAIM_W, CLAIM_H }; }

    Rect LaneCard(int lane)
    {
        const int inner = MID_W - 2 * MID_MARGIN;
        const int width = (inner - (Commander::LANE_COUNT - 1) * LANE_GAP) / Commander::LANE_COUNT;
        return { MID_X + MID_MARGIN + lane * (width + LANE_GAP), CON_Y + LANE_ROW_Y, width, LANE_H };
    }

    Rect CommandButton(int index)
    {
        const int column = index % BTN_COLS;
        const int row = index / BTN_COLS;
        return { BTN_ORIGIN_X + column * (BTN_W + BTN_GAP_X), BTN_ORIGIN_Y + row * (BTN_H + BTN_GAP_Y), BTN_W, BTN_H };
    }

    Rect ReleaseButton() { return { MID_X + MID_W - MID_MARGIN - RELEASE_W, CON_Y + HINT_ROW_Y, RELEASE_W, HINT_H }; }

    bool IsUnitCommand(Command command)
    {
        return command == Command::Unit0 || command == Command::Unit1 || command == Command::Unit2;
    }

    int UnitOf(Command command)
    {
        return static_cast<int>(command) - static_cast<int>(Command::Unit0);
    }

    bool CanBuyIncome(const Commander::State& state)
    {
        return state.nextIncomeCost > 0 && state.essence >= state.nextIncomeCost;
    }

    bool CanAfford(const Commander::State& state, int unit)
    {
        return unit < state.unitCount && state.essence >= state.unitCost[unit];
    }

    int QueuedTotal(const Commander::State& state)
    {
        int total = 0;
        for (int lane = 0; lane < Commander::LANE_COUNT; ++lane)
        {
            for (int unit = 0; unit < state.unitCount; ++unit)
                total += state.queued[lane][unit];
        }
        return total;
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

    // Buys a pack for the active lane (Shift = a burst); each order is paid separately, so a burst stops when the essence runs out.
    // The creatures wait in the base until the wave timer (or "send now") releases them.
    void SendPack(const Commander::State& state, int unit, bool burst)
    {
        const int lane = Commander::ActiveLane();
        if (unit >= state.unitCount || !Commander::IsLaneOpen(state, lane) || !CanAfford(state, unit))
            return;

        const int times = burst ? std::min(SHIFT_BURST, state.essence / std::max(1, state.unitCost[unit])) : 1;
        for (int i = 0; i < times; ++i)
            Commander::Order(Commander::Action::Send, unit, lane);

        s_flashUntil[unit] = NowMs() + FLASH_MS;
        swprintf(s_note, std::size(s_note), L"En espera: %ls x%d  ->  %ls", UnitName(unit), times, LANE_NAMES[lane]);
        s_noteUntil = NowMs() + SENT_NOTE_MS;
    }

    void BuyIncome(const Commander::State& state)
    {
        if (!CanBuyIncome(state))
            return;

        Commander::Order(Commander::Action::Income);
        s_flashUntil[static_cast<int>(Command::Income)] = NowMs() + FLASH_MS;
        Note(L"Ingreso mejorado");
    }

    // "Send now": the creatures held in the base leave at once; the wave timer keeps running. The server keeps a cooldown.
    void SendNow()
    {
        const int cooldown = Commander::SendNowSecondsLeft();
        if (cooldown > 0)
        {
            wchar_t text[64] = {};
            swprintf(text, std::size(text), L"Enviar ya: espera %d s", cooldown);
            Note(text);
            return;
        }

        Commander::Order(Commander::Action::SendNow);
        s_flashUntil[static_cast<int>(Command::SendNow)] = NowMs() + FLASH_MS;
        Note(L"Enviar ya");
    }

    // A click or key on a slot of the card.
    void UseCommand(const Commander::State& state, int index, bool burst)
    {
        const Command command = COMMANDS[index];
        if (IsUnitCommand(command))
            SendPack(state, UnitOf(command), burst);
        else if (command == Command::Income)
            BuyIncome(state);
        else if (command == Command::SendNow)
            SendNow();
        // The other slots are reserved: they do nothing yet.
    }

    void PointCameraAtLane(int lane)
    {
        float tileX = 0.f;
        float tileY = 0.f;
        Commander::LaneFocusTile(lane, tileX, tileY);
        CameraManager::Instance().FocusMobaCameraOn(tileX * TERRAIN_SCALE, tileY * TERRAIN_SCALE);
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

    // "Proxima oleada 0:23": everyone in the match sees it, commander or not.
    void RenderWaveTimer(const Commander::State& state)
    {
        if (state.waveInterval <= 0)
            return;

        const int seconds = Commander::WaveSecondsLeft();
        const bool warn = seconds <= WARN_SECONDS;
        const Rect box = TimerBox();
        if (warn)
            Draw::FillRect(box, 0.20f, 0.12f, 0.03f, 0.90f);
        else
            Draw::FillRect(box, 0.04f, 0.055f, 0.086f, 0.86f);
        Draw::FrameRect(box, warn ? 0.96f : 0.78f, warn ? 0.75f : 0.64f, warn ? 0.29f : 0.29f, 1.f);
        EndRenderColor();

        wchar_t line[64] = {};
        swprintf(line, std::size(line), L"Pr\u00f3xima oleada  %d:%02d", seconds / 60, seconds % 60);
        const int textY = box.y + (box.h - Draw::BodyLineHeight()) / 2;
        if (warn)
            Draw::PutTextCentered(Draw::Bold(), box.x, textY, box.w, line, 255, 205, 100);
        else
            Draw::PutTextCentered(Draw::Bold(), box.x, textY, box.w, line, 255, 244, 214);
    }

    // One card per lane: the lane key and name, the creatures alive (yours vs the rival's, amber when the rival presses the lane) and,
    // below, the packs waiting in the base by type.
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

        const int lineH = Draw::BodyLineHeight();
        const int headerY = r.y + 3;
        wchar_t label[24] = {};
        swprintf(label, std::size(label), L"%lc %ls", LANE_KEYS[lane], LANE_NAMES[lane]);
        if (!open)
        {
            Draw::PutTextLeft(Draw::Bold(), r.x + 6, headerY, 50, label, 90, 98, 115);
            return;
        }

        Draw::PutTextLeft(Draw::Bold(), r.x + 6, headerY, 50, label, active ? 255 : 220, active ? 244 : 224, active ? 214 : 200);
        wchar_t counts[32] = {};
        swprintf(counts, std::size(counts), L"%d vs %d", state.ownCreeps[lane], state.enemyCreeps[lane]);
        if (pressed)
            Draw::PutText(Draw::Body(), r.x + 50, headerY, r.w - 56, counts, 255, 200, 90, RT3_SORT_RIGHT);
        else
            Draw::PutText(Draw::Body(), r.x + 50, headerY, r.w - 56, counts, 190, 198, 212, RT3_SORT_RIGHT);

        // The waiting zone of the lane: one line per type of pack.
        const int unitLines = std::min(state.unitCount, static_cast<int>(std::size(UNIT_NAMES)));
        for (int unit = 0; unit < unitLines; ++unit)
        {
            const int rowY = headerY + (unit + 1) * lineH + 3;
            const int held = state.queued[lane][unit];
            wchar_t amount[16] = {};
            swprintf(amount, std::size(amount), L"x%d", held);
            const BYTE value = held > 0 ? 241 : 100;
            Draw::PutTextLeft(Draw::Body(), r.x + 8, rowY, r.w - 16, UnitName(unit), held > 0 ? 220 : 110, held > 0 ? 226 : 118, held > 0 ? 238 : 135);
            Draw::PutText(Draw::Body(), r.x + 8, rowY, r.w - 16, amount, value, held > 0 ? 196 : 108, held > 0 ? 83 : 120, RT3_SORT_RIGHT);
        }
    }

    void RenderCommandButton(const Commander::State& state, int index)
    {
        const Command command = COMMANDS[index];
        const Rect r = CommandButton(index);
        const bool reserved = !IsUnitCommand(command) && command != Command::Income && command != Command::SendNow;

        bool enabled = false;
        wchar_t top[40] = {};
        wchar_t bottom[24] = {};
        if (IsUnitCommand(command))
        {
            const int unit = UnitOf(command);
            enabled = CanAfford(state, unit) && Commander::IsLaneOpen(state, Commander::ActiveLane());
            swprintf(top, std::size(top), L"%lc %ls", COMMAND_KEYS[index], UnitName(unit));
            if (unit < state.unitCount)
                swprintf(bottom, std::size(bottom), L"%d", state.unitCost[unit]);
        }
        else if (command == Command::Income)
        {
            enabled = CanBuyIncome(state);
            swprintf(top, std::size(top), L"%lc Ingreso", COMMAND_KEYS[index]);
            if (state.nextIncomeCost > 0)
                swprintf(bottom, std::size(bottom), L"%d", state.nextIncomeCost);
            else
                swprintf(bottom, std::size(bottom), L"M\u00e1ximo");
        }
        else if (command == Command::SendNow)
        {
            const int cooldown = Commander::SendNowSecondsLeft();
            enabled = cooldown <= 0;
            swprintf(top, std::size(top), L"%lc Enviar ya", COMMAND_KEYS[index]);
            if (cooldown > 0)
                swprintf(bottom, std::size(bottom), L"%d s", cooldown);
            else
                swprintf(bottom, std::size(bottom), L"%d en espera", QueuedTotal(state));
        }
        else
        {
            swprintf(top, std::size(top), L"%lc", COMMAND_KEYS[index]);
            swprintf(bottom, std::size(bottom), L"Pronto");
        }

        const bool hot = enabled && Draw::MouseInRect(r);
        const bool flash = NowMs() < s_flashUntil[index];
        const float shade = flash ? 1.9f : (hot ? HOT_SHADE : IDLE_SHADE);
        if (enabled)
            Draw::FillRect(r, 0.10f * shade, 0.20f * shade, 0.34f * shade, 0.95f);
        else
            Draw::FillRect(r, reserved ? 0.07f : 0.09f, reserved ? 0.08f : 0.11f, reserved ? 0.11f : 0.16f, 0.95f);
        Draw::FrameRect(r, enabled ? 0.31f : 0.23f, enabled ? 0.71f : 0.27f, enabled ? 1.0f : 0.35f, 1.f);
        EndRenderColor();

        const int lineH = Draw::BodyLineHeight();
        const int topY = r.y + std::max(1, (r.h - 2 * lineH) / 2);
        if (enabled)
        {
            Draw::PutTextCentered(Draw::Body(), r.x, topY, r.w, top, 255, 244, 214);
            Draw::PutTextCentered(Draw::Body(), r.x, topY + lineH, r.w, bottom, 241, 196, 83);
        }
        else
        {
            const BYTE grey = reserved ? 86 : 110;
            Draw::PutTextCentered(Draw::Body(), r.x, topY, r.w, top, grey, grey + 8, grey + 25);
            Draw::PutTextCentered(Draw::Body(), r.x, topY + lineH, r.w, bottom, grey, grey + 8, grey + 25);
        }
    }

    void RenderHintRow()
    {
        const Rect release = ReleaseButton();
        const bool hot = Draw::MouseInRect(release);
        const int width = MID_W - 2 * MID_MARGIN - RELEASE_W;
        if (NowMs() < s_noteUntil)
            Draw::PutTextLeft(Draw::Body(), MID_X + MID_MARGIN, release.y, width, s_note, 127, 212, 255);
        else
            Draw::PutTextLeft(Draw::Body(), MID_X + MID_MARGIN, release.y, width, L"May\u00fas = x5    F1-F3 = c\u00e1mara", 140, 150, 168);
        Draw::PutTextCentered(Draw::Body(), release.x, release.y, release.w, L"Soltar el mando", hot ? 255 : 170, hot ? 220 : 179, hot ? 170 : 194);
    }

    void RenderConsole(const Commander::State& state)
    {
        EnsureOpenActiveLane(state);

        // The minimap draws its own backing in the left box; the console covers the rest of the width.
        const Rect queue = QueuePanel();
        const Rect card = CardPanel();
        Draw::FillRect(queue, 0.04f, 0.055f, 0.086f, 0.94f);
        Draw::FillRect(card, 0.04f, 0.055f, 0.086f, 0.94f);
        Draw::FillRect({ queue.x, queue.y, queue.w + card.w, 1 }, 0.78f, 0.64f, 0.29f, 1.f);
        Draw::FillRect({ card.x, card.y, 1, card.h }, 0.78f, 0.64f, 0.29f, 1.f);
        EndRenderColor();

        for (int lane = 0; lane < Commander::LANE_COUNT; ++lane)
            RenderLaneCard(state, lane);
        for (int index = 0; index < BUTTON_COUNT; ++index)
            RenderCommandButton(state, index);
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
        for (int index = 0; index < BUTTON_COUNT; ++index)
        {
            if (!Draw::MouseInRect(CommandButton(index)))
                continue;

            UseCommand(state, index, burst);
            return true;
        }

        if (Draw::MouseInRect(ReleaseButton()))
        {
            Commander::Order(Commander::Action::Release);
            return true;
        }

        return false;
    }

    // The chat goes up above the console while the RTS view is on (it would sit under it otherwise), and comes back afterwards.
    void MoveChat(bool raised)
    {
        const int shift = raised ? CHAT_RAISE : 0;
        if (g_pChatListBox != nullptr)
            g_pChatListBox->SetPosition(0, CHAT_LOG_Y - shift);
        if (g_pChatInputBox != nullptr)
            g_pChatInputBox->SetWndPos(0, CHAT_INPUT_Y - shift);
    }

    // Entering the RTS view closes the hero windows that were open; leaving it puts the chat back.
    void OnRtsViewChanged(bool rts)
    {
        MoveChat(rts);
        if (!rts || g_pNewUISystem == nullptr)
            return;

        g_pNewUISystem->Hide(INTERFACE_INVENTORY);
        g_pNewUISystem->Hide(INTERFACE_CHARACTER);
        s_noteUntil = 0.0;
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
    RenderWaveTimer(state);
    if (state.isCommander)
        RenderConsole(state);
    else if (state.commandFree)
        RenderClaim();
    if (Commander::RtsView())
        RenderMobaScoreboardOverlay();   // the main frame that draws the TAB scoreboard is hidden in the RTS view
    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaCommanderWindow::Update()
{
    const bool rts = Commander::RtsView();
    if (rts != s_rtsShown)
    {
        s_rtsShown = rts;
        OnRtsViewChanged(rts);
    }
    return true;
}

bool CMobaCommanderWindow::UpdateMouseEvent()
{
    if (Draw::HeldClickSwallowed())
        return false;

    const Commander::State& state = Commander::Get();
    if (!state.visible)
        return true;

    // The minimap owns the left end of the console.
    const bool overMinimap = state.isCommander && Draw::MouseInRect(MinimapBox());
    if (overMinimap)
        return true;

    const bool overConsole = state.isCommander && Draw::MouseInRect(Console());
    const bool overClaim = !state.isCommander && state.commandFree && Draw::MouseInRect(ClaimButton());
    const bool overTop = Draw::MouseInRect(TopBar()) || (state.waveInterval > 0 && Draw::MouseInRect(TimerBox()));
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
    if (!Commander::RtsView())
        return true;

    const Commander::State& state = Commander::Get();
    EnsureOpenActiveLane(state);

    // F1/F2/F3 put the camera on the top, middle and bottom lane.
    for (int lane = 0; lane < Commander::LANE_COUNT; ++lane)
    {
        if (IsPress(VK_F1 + lane))
        {
            PointCameraAtLane(lane);
            return false;
        }
    }

    // 1/2/3 pick the lane.
    for (int lane = 0; lane < Commander::LANE_COUNT; ++lane)
    {
        if (IsPress('1' + lane))
        {
            if (Commander::IsLaneOpen(state, lane))
                Commander::SetActiveLane(lane);
            return false;
        }
    }

    // The command card: Q/W/E packs (Shift = a burst of five), A income, X send now; S, D, Z and C are reserved (taken, but they do nothing).
    const bool burst = Core::Input::IsKeyDown(VK_SHIFT);
    for (int index = 0; index < BUTTON_COUNT; ++index)
    {
        if (IsPress(static_cast<int>(COMMAND_KEYS[index])))
        {
            UseCommand(state, index, burst);
            return false;
        }
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
