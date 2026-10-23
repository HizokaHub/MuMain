#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaSummonHud.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaSummon.h"

#include <algorithm>

using namespace SEASON3B;
namespace Summon = World::Moba::Summon;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen: just above the left end of the skill bar.
    constexpr int STRIP_X = 222;
    constexpr int STRIP_Y = 398;
    constexpr int STRIP_W = 200;
    constexpr int STRIP_H = 28;
    constexpr int BAR_X = 8;
    constexpr int BAR_Y = 16;
    constexpr int BAR_W = 130;
    constexpr int BAR_H = 7;
    constexpr int LABEL_Y = 1;

    constexpr float LAYER_DEPTH = 10.2f;
    constexpr float KEY_EVENT_ORDER = 50.0f;
    constexpr DWORD UI_KEY_OFFSET = 9;

    Rect Strip() { return { STRIP_X, STRIP_Y, STRIP_W, STRIP_H }; }
}

void CMobaSummonHud::Create(CNewUIManager* pNewUIMng)
{
    static CMobaSummonHud s_hud;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_hud);
}

bool CMobaSummonHud::Render()
{
    const Summon::State& state = Summon::Get();
    if (!state.present)
        return true;

    const DWORD backColor = g_pRenderText->GetTextColor();
    const bool control = Summon::Controlling();
    const Rect strip = Strip();
    Draw::FillRect(strip, 0.04f, 0.055f, 0.086f, 1.f);
    Draw::FrameRect(strip, control ? 0.95f : 0.78f, control ? 0.80f : 0.64f, control ? 0.40f : 0.29f, 1.f);

    const Rect bar = { strip.x + BAR_X, strip.y + BAR_Y, BAR_W, BAR_H };
    const int fill = bar.w * std::clamp(state.hpPercent, 0, 100) / 100;
    Draw::FillRect(bar, 0.10f, 0.11f, 0.14f, 1.f);
    Draw::FillRect({ bar.x, bar.y, fill, bar.h }, state.hpPercent > 35 ? 0.20f : 0.78f, state.hpPercent > 35 ? 0.70f : 0.22f, 0.25f, 1.f);
    Draw::FrameRect(bar, 0.23f, 0.27f, 0.35f, 1.f);
    EndRenderColor();

    wchar_t text[64] = {};
    if (state.level >= 10)
        swprintf(text, std::size(text), L"Invocaci\u00f3n  nv 10 MAX  rango %d", state.rank);
    else
        swprintf(text, std::size(text), L"Invocaci\u00f3n  nv %d (%d%%)  rango %d", state.level, state.progress, state.rank);
    Draw::PutTextLeft(Draw::Bold(), strip.x + BAR_X, strip.y + LABEL_Y, BAR_W, text, 233, 228, 212);
    Draw::PutTextCentered(Draw::Bold(), strip.x + BAR_X + BAR_W + 4, strip.y + 5, STRIP_W - BAR_X - BAR_W - 8, control ? L"[X] MANDO" : (state.stay ? L"[Z] llamar" : L"[X] mandar"),
        control ? 255 : 160, control ? 220 : 170, control ? 90 : 185);
    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaSummonHud::Update()
{
    return true;
}

bool CMobaSummonHud::UpdateMouseEvent()
{
    if (Draw::HeldClickSwallowed())
        return false;

    if (!Summon::Get().present || !Draw::MouseInRect(Strip()))
        return true;

    if (IsPress(VK_LBUTTON))
    {
        Draw::SwallowHeldClick();
        Summon::ToggleControl();
    }

    // The strip swallows the mouse: a click on it must not walk the hero.
    return false;
}

bool CMobaSummonHud::UpdateKeyEvent()
{
    return true;
}

float CMobaSummonHud::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaSummonHud::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
