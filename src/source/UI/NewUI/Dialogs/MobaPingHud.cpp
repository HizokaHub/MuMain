#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaPingHud.h"

#include "Camera/CameraManager.h"              // g_Camera
#include "Camera/CameraProjection.h"           // WorldToScreen
#include "Engine/Object/ZzzCharacter.h"        // Hero
#include "Network/Server/WSclient.h"           // g_MobaLevel, g_MyMobaTeam
#include "Render/Terrain/ZzzLodTerrain.h"      // RequestTerrainHeight, TERRAIN_SCALE
#include "Render/Textures/ZzzOpenglUtil.h"     // CollisionPosition, MouseX
#include "UI/Legacy/UIControls.h"              // g_pRenderText
#include "UI/NewUI/Dialogs/MobaPingArt.h"
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "UI/NewUI/HUD/NewUIMiniMap.h"
#include "UI/NewUI/NewUICommon.h"              // IsPress, IsRepeat, IsRelease
#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUISystem.h"              // g_pNewUIMiniMap
#include "World/MapInfra/MapManager.h"         // gMapManager, WD_200_MOBA_ARENA
#include "World/Moba/MobaPing.h"

#include <algorithm>
#include <cmath>

using namespace SEASON3B;
namespace Ping = World::Moba::Ping;
namespace Art = UI::Moba::PingArt;
namespace Draw = UI::Moba::Draw;

extern double WorldTime;

namespace
{
    constexpr float LAYER_DEPTH = 10.3f;
    constexpr float KEY_EVENT_ORDER = 60.0f;
    constexpr DWORD UI_KEY_OFFSET = 13;

    // The area of the screen the world is drawn in (the bottom bar covers the rest), in the 640x480 reference screen.
    constexpr float VIEW_CENTRE_X = 320.f;
    constexpr float VIEW_CENTRE_Y = 215.f;
    constexpr float VIEW_BOTTOM = 430.f;
    constexpr float EDGE_MARGIN = 30.f;

    // The ping wheel: the nine buttons around the click.
    constexpr double WHEEL_DELAY_MS = 180.0;
    constexpr float WHEEL_RADIUS = 58.f;           // from the centre to the outer buttons, in reference px along X (square on screen)
    constexpr float BUTTON_WIDTH = 34.f;
    constexpr float CENTRE_BUTTON_WIDTH = 42.f;
    constexpr float HOVER_SCALE = 1.3f;
    constexpr float BACKDROP_WIDTH = 190.f;
    constexpr float LABEL_OFFSET = 44.f;
    constexpr int SLOT_COUNT = 8;
    constexpr float SLOT_DEGREES = 45.f;
    constexpr float PI = 3.14159265f;

    // The marks on the ground.
    constexpr float ICON_WIDTH = 34.f;
    constexpr float GLOW_WIDTH = 64.f;
    constexpr float RIPPLE_MIN = 16.f;
    constexpr float RIPPLE_MAX = 100.f;
    constexpr double RIPPLE_PERIOD_MS = 900.0;
    constexpr double POP_MS = 220.0;
    constexpr double FADE_MS = 500.0;
    constexpr double NAME_MS = 1600.0;
    constexpr float BOB_PIXELS = 3.f;
    constexpr float HEIGHT_ABOVE_GROUND = 70.f;
    constexpr float EDGE_ICON_WIDTH = 24.f;
    constexpr float EDGE_ARROW_WIDTH = 30.f;
    constexpr float EDGE_ICON_INSET = 24.f;
    constexpr float NAME_OFFSET_Y = 22.f;
    constexpr float NAME_BOX_WIDTH = 140.f;
    constexpr int MAP_LIMIT = 255;

    enum Phase
    {
        IDLE,       // no gesture
        HELD,       // a ping key and the button are down; the wheel has not opened yet
        WHEEL,      // the wheel is open
    };

    struct Gesture
    {
        Phase phase = IDLE;
        double pressedMs = 0.0;
        float centreX = 0.f;      // the click (the centre of the wheel), reference px
        float centreY = 0.f;
        int tileX = 0;
        int tileY = 0;
        Ping::Kind defaultKind = Ping::KIND_GENERIC;
        Ping::Kind selected = Ping::KIND_CAUTION;
    } s_gesture;

    bool InArena()
    {
        return Hero != nullptr && g_MobaLevel > 0 && gMapManager.WorldActive == WD_200_MOBA_ARENA;
    }

    // Reference px of the vertical axis that look as long on screen as <along> px of the horizontal axis.
    float ToRefY(float along)
    {
        return along * g_fScreenRate_x / g_fScreenRate_y;
    }

    // The tile the click points at: the one under the cursor on the minimap, or on the ground.
    bool ResolveTile(int& tileX, int& tileY)
    {
        float minimapX = 0.f, minimapY = 0.f;
        if (g_pNewUIMiniMap != nullptr && g_pNewUIMiniMap->MobaMouseTile(minimapX, minimapY))
        {
            tileX = static_cast<int>(minimapX);
            tileY = static_cast<int>(minimapY);
            return tileX >= 0 && tileX <= MAP_LIMIT && tileY >= 0 && tileY <= MAP_LIMIT;
        }

        tileX = static_cast<int>(CollisionPosition[0] / TERRAIN_SCALE);
        tileY = static_cast<int>(CollisionPosition[1] / TERRAIN_SCALE);
        return tileX >= 0 && tileX <= MAP_LIMIT && tileY >= 0 && tileY <= MAP_LIMIT;
    }

    void StartGesture()
    {
        s_gesture.phase = HELD;
        s_gesture.pressedMs = WorldTime;
        s_gesture.centreX = static_cast<float>(MouseX);
        s_gesture.centreY = static_cast<float>(MouseY);
        s_gesture.defaultKind = Ping::DefaultKind();
        s_gesture.selected = Ping::KIND_CAUTION;
    }

    // The wheel stays inside the part of the screen the world is drawn in.
    void OpenWheel()
    {
        const float reachX = WHEEL_RADIUS + BUTTON_WIDTH;
        const float reachY = ToRefY(reachX);
        s_gesture.centreX = std::clamp(s_gesture.centreX, reachX, 640.f - reachX);
        s_gesture.centreY = std::clamp(s_gesture.centreY, reachY, VIEW_BOTTOM - reachY);
        s_gesture.phase = WHEEL;
        s_gesture.selected = Ping::KIND_CAUTION;
        Ping::PlayWheelOpen();
    }

    void UpdateSelection()
    {
        const float dx = static_cast<float>(MouseX) - s_gesture.centreX;
        const float dy = (static_cast<float>(MouseY) - s_gesture.centreY) * g_fScreenRate_y / g_fScreenRate_x;
        const Ping::Kind kind = Ping::WheelKind(dx, dy);
        if (kind != s_gesture.selected)
        {
            s_gesture.selected = kind;
            Ping::PlayWheelTick();
        }
    }

    void Finish(bool send)
    {
        if (send)
            Ping::Send(s_gesture.phase == WHEEL ? s_gesture.selected : s_gesture.defaultKind, s_gesture.tileX, s_gesture.tileY);
        s_gesture.phase = IDLE;
    }

    float SmoothStep(float t)
    {
        t = std::clamp(t, 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    }

    // The size of a mark during its pop-in: it grows from 40 % and overshoots a little before it settles at 100 %.
    float PopScale(double ageMs)
    {
        const float t = static_cast<float>(std::clamp(ageMs / POP_MS, 0.0, 1.0));
        return 0.4f + 0.6f * SmoothStep(t) + 0.3f * std::sin(t * PI);
    }

    float MarkAlpha(double ageMs)
    {
        const double left = Ping::GROUND_MS - ageMs;
        return static_cast<float>(std::clamp(left / FADE_MS, 0.0, 1.0));
    }

    // Where a mark is on the screen, in reference px; false when the point is behind the camera.
    bool ProjectTile(int tileX, int tileY, float& sx, float& sy)
    {
        vec3_t position;
        position[0] = (static_cast<float>(tileX) + 0.5f) * TERRAIN_SCALE;
        position[1] = (static_cast<float>(tileY) + 0.5f) * TERRAIN_SCALE;
        position[2] = RequestTerrainHeight(position[0], position[1]) + HEIGHT_ABOVE_GROUND;
        int x = 0, y = 0;
        CameraProjection::WorldToScreen(g_Camera, position, &x, &y);
        sx = static_cast<float>(x);
        sy = static_cast<float>(y);
        return true;
    }

    bool OnScreen(float sx, float sy)
    {
        return sx >= EDGE_MARGIN && sx <= 640.f - EDGE_MARGIN && sy >= EDGE_MARGIN && sy <= VIEW_BOTTOM - EDGE_MARGIN;
    }

    void DrawRipples(const Ping::Mark& mark, float sx, float sy, double age, float alpha)
    {
        const Art::Tint tint = Art::KindColor(mark.kind);
        for (int k = 0; k < 2; ++k)
        {
            const double phase = std::fmod(age / RIPPLE_PERIOD_MS + 0.5 * k, 1.0);
            const float width = RIPPLE_MIN + (RIPPLE_MAX - RIPPLE_MIN) * static_cast<float>(phase);
            Art::DrawCell(Art::CELL_RING, sx, sy, width, 0.f, tint, (1.f - static_cast<float>(phase)) * 0.9f * alpha);
        }
    }

    void DrawGroundMark(const Ping::Mark& mark)
    {
        const double age = WorldTime - mark.atMs;
        if (age > Ping::GROUND_MS)
            return;

        float sx = 0.f, sy = 0.f;
        if (!ProjectTile(mark.tileX, mark.tileY, sx, sy))
            return;

        const float alpha = MarkAlpha(age);
        if (OnScreen(sx, sy))
        {
            const float bob = std::sin(static_cast<float>(age) * 0.008f) * BOB_PIXELS;
            const float width = ICON_WIDTH * PopScale(age);
            DrawRipples(mark, sx, sy, age, alpha);
            Art::DrawCell(Art::CELL_GLOW, sx, sy - bob, GLOW_WIDTH, 0.f, Art::KindColor(mark.kind), 0.55f * alpha);
            Art::DrawKind(mark.kind, sx, sy - bob, width, alpha);
            return;
        }

        // Out of view: an arrow at the edge of the screen pointing at it, with a small icon next to it.
        const float dx = sx - VIEW_CENTRE_X;
        const float dy = sy - VIEW_CENTRE_Y;
        const float length = std::max(std::sqrt(dx * dx + dy * dy), 1.f);
        const float limitX = VIEW_CENTRE_X - EDGE_MARGIN;
        const float limitY = VIEW_CENTRE_Y - EDGE_MARGIN;
        const float scale = std::min(limitX / std::max(std::fabs(dx), 1.f), limitY / std::max(std::fabs(dy), 1.f));
        const float ex = VIEW_CENTRE_X + dx * scale;
        const float ey = VIEW_CENTRE_Y + dy * scale;
        const float compass = std::atan2(dx, -dy) * 180.f / PI;
        const float inward = EDGE_ICON_INSET / length;
        Art::DrawCell(Art::CELL_ARROW, ex, ey, EDGE_ARROW_WIDTH, compass, Art::KindColor(mark.kind), alpha);
        Art::DrawKind(mark.kind, ex - dx * inward * 1.f, ey - dy * inward * 1.f, EDGE_ICON_WIDTH, alpha);
    }

    void DrawMarkNames()
    {
        const DWORD backColor = g_pRenderText->GetTextColor();
        for (const Ping::Mark& mark : Ping::Marks())
        {
            const double age = WorldTime - mark.atMs;
            if (age > NAME_MS || mark.sender.empty())
                continue;

            float sx = 0.f, sy = 0.f;
            if (!ProjectTile(mark.tileX, mark.tileY, sx, sy) || !OnScreen(sx, sy))
                continue;

            const Art::Tint tint = Art::KindColor(mark.kind);
            Draw::PutTextCentered(Draw::Body(), static_cast<int>(sx - NAME_BOX_WIDTH * 0.5f), static_cast<int>(sy + ToRefY(NAME_OFFSET_Y)), static_cast<int>(NAME_BOX_WIDTH),
                mark.sender.c_str(), static_cast<BYTE>(120 + 135 * tint.r), static_cast<BYTE>(120 + 135 * tint.g), static_cast<BYTE>(120 + 135 * tint.b));
        }
        g_pRenderText->SetTextColor(backColor);
    }

    // The nine buttons: the eight outer ones clockwise from the top (the slots of Ping::WheelKind) and the caution button in the centre.
    constexpr Ping::Kind SLOTS[SLOT_COUNT] = {
        Ping::KIND_RETREAT, Ping::KIND_PUSH, Ping::KIND_ON_MY_WAY, Ping::KIND_ALL_IN, Ping::KIND_ASSIST, Ping::KIND_NEED_VISION,
        Ping::KIND_ENEMY_MISSING, Ping::KIND_ENEMY_VISION,
    };

    void DrawButton(Ping::Kind kind, float cx, float cy, float width, bool selected)
    {
        const Art::Tint tint = Art::KindColor(kind);
        const float size = selected ? width * HOVER_SCALE : width;
        if (selected)
            Art::DrawCell(Art::CELL_GLOW, cx, cy, size * 2.1f, 0.f, tint, 0.85f);
        Art::DrawCell(Art::CELL_DISC, cx, cy, size + 4.f, 0.f, { 0.07f, 0.04f, 0.14f }, 0.88f);
        Art::DrawKind(kind, cx, cy, size, selected ? 1.f : 0.9f);
        Art::DrawCell(Art::CELL_BORDER, cx, cy, size + 2.f, 0.f, tint, selected ? 1.f : 0.7f);
    }

    void DrawWheel()
    {
        const float cx = s_gesture.centreX;
        const float cy = s_gesture.centreY;
        Art::DrawCell(Art::CELL_GLOW, cx, cy, BACKDROP_WIDTH, 0.f, { 0.45f, 0.28f, 0.95f }, 0.30f);
        for (int slot = 0; slot < SLOT_COUNT; ++slot)
        {
            const float radians = static_cast<float>(slot) * SLOT_DEGREES * PI / 180.f;
            const float bx = cx + std::sin(radians) * WHEEL_RADIUS;
            const float by = cy - std::cos(radians) * ToRefY(WHEEL_RADIUS);
            DrawButton(SLOTS[slot], bx, by, BUTTON_WIDTH, SLOTS[slot] == s_gesture.selected);
        }
        DrawButton(Ping::KIND_CAUTION, cx, cy, CENTRE_BUTTON_WIDTH, s_gesture.selected == Ping::KIND_CAUTION);
    }

    void DrawWheelLabel()
    {
        const DWORD backColor = g_pRenderText->GetTextColor();
        const float y = s_gesture.centreY + ToRefY(WHEEL_RADIUS + LABEL_OFFSET);
        Draw::PutTextCentered(Draw::Bold(), static_cast<int>(s_gesture.centreX - 90.f), static_cast<int>(y), 180, Ping::NameOf(s_gesture.selected), 255, 235, 150);
        g_pRenderText->SetTextColor(backColor);
    }
}

void CMobaPingHud::Create(CNewUIManager* pNewUIMng)
{
    static CMobaPingHud s_hud;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_hud);
}

bool CMobaPingHud::Render()
{
    if (!InArena() || !Art::Loaded())
        return true;

    const std::vector<Ping::Mark>& marks = Ping::Marks();
    const bool anyMark = !marks.empty();
    if (!anyMark && s_gesture.phase != WHEEL)
        return true;

    Art::Begin();
    for (const Ping::Mark& mark : marks)
        DrawGroundMark(mark);
    if (s_gesture.phase == WHEEL)
        DrawWheel();
    Art::End();

    DrawMarkNames();
    if (s_gesture.phase == WHEEL)
        DrawWheelLabel();
    return true;
}

bool CMobaPingHud::Update()
{
    return true;
}

bool CMobaPingHud::UpdateMouseEvent()
{
    if (!InArena())
    {
        s_gesture.phase = IDLE;
        return true;
    }

    if (s_gesture.phase == IDLE)
    {
        if (!IsPress(VK_LBUTTON) || !Ping::ModifierHeld())
            return true;

        if (!ResolveTile(s_gesture.tileX, s_gesture.tileY))
            return true;

        StartGesture();
        return false;
    }

    // A gesture is under way: the world must not see the mouse until the button is released.
    if (IsPress(VK_RBUTTON))
    {
        Finish(false);
        return false;
    }

    if (s_gesture.phase == HELD && IsRepeat(VK_LBUTTON) && WorldTime - s_gesture.pressedMs >= WHEEL_DELAY_MS)
        OpenWheel();

    if (s_gesture.phase == WHEEL)
        UpdateSelection();

    if (IsRelease(VK_LBUTTON))
        Finish(true);

    return false;
}

bool CMobaPingHud::UpdateKeyEvent()
{
    return true;
}

float CMobaPingHud::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaPingHud::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
