#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaSummonHud.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "UI/NewUI/Dialogs/MobaPetHud.h"
#include "UI/NewUI/HUD/Skills/SkillIcon.h"
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
    constexpr int SKILL_SIZE = 28;      // the skill buttons, to the right of the strip
    constexpr int SKILL_GAP = 3;
    constexpr int ICON_INSET = 2;
    constexpr float COOLDOWN_SHADE = 0.72f;
    constexpr float LOCKED_SHADE = 0.60f;
    constexpr double MS_PER_SECOND = 1000.0;
    constexpr unsigned short NO_TARGET = 0xFFFF;

    constexpr float LAYER_DEPTH = 10.2f;
    constexpr float KEY_EVENT_ORDER = 50.0f;
    constexpr DWORD UI_KEY_OFFSET = 9;

    Rect Strip() { return { STRIP_X, STRIP_Y - SEASON3B::CMobaPetHud::ReservedHeight(), STRIP_W, STRIP_H }; }

    Rect SkillRect(int slot)
    {
        const Rect strip = Strip();
        return { strip.x + strip.w + SKILL_GAP + slot * (SKILL_SIZE + SKILL_GAP), strip.y, SKILL_SIZE, SKILL_SIZE };
    }

    void RenderSkill(int slot, bool control)
    {
        const Rect button = SkillRect(slot);
        const Summon::SkillInfo& skill = Summon::Skill(slot);
        const bool locked = Summon::Get().rank < skill.minRank;
        Draw::FillRect(button, 0.04f, 0.055f, 0.086f, 1.f);
        Draw::FrameRect(button, control ? 0.95f : 0.50f, control ? 0.80f : 0.42f, control ? 0.40f : 0.20f, 1.f);
        EndRenderColor();
        const float inner = static_cast<float>(SKILL_SIZE - 2 * ICON_INSET);
        UI::Skills::Icon::Render(skill.iconSkill, static_cast<float>(button.x + ICON_INSET), static_cast<float>(button.y + ICON_INSET), inner, inner);

        const Rect face = { button.x + ICON_INSET, button.y + ICON_INSET, SKILL_SIZE - 2 * ICON_INSET, SKILL_SIZE - 2 * ICON_INSET };
        if (locked || !control)
        {
            Draw::FillRect(face, 0.f, 0.f, 0.f, locked ? LOCKED_SHADE : 0.35f);
            EndRenderColor();
        }

        const double remainMs = Summon::SkillRemainingMs(slot);
        if (remainMs > 0.0 && !locked)
        {
            const double fullMs = std::max(skill.cooldownSeconds * MS_PER_SECOND, remainMs);
            const int sweep = static_cast<int>(static_cast<double>(face.h) * std::clamp(remainMs / fullMs, 0.0, 1.0));
            Draw::FillRect({ face.x, face.y + (face.h - sweep), face.w, sweep }, 0.f, 0.f, 0.f, COOLDOWN_SHADE);
            EndRenderColor();
            wchar_t seconds[16] = {};
            swprintf(seconds, std::size(seconds), L"%d", static_cast<int>(remainMs / MS_PER_SECOND) + 1);
            Draw::PutTextCentered(Draw::Bold(), button.x, button.y + 8, SKILL_SIZE, seconds, 255, 255, 230);
        }

        wchar_t key[8] = {};
        swprintf(key, std::size(key), L"%d", slot + 1);
        Draw::PutTextLeft(Draw::Bold(), button.x + 3, button.y + 1, SKILL_SIZE, key, 255, 255, 255);
    }

    // The skill button under the mouse, or -1.
    int SkillUnderMouse()
    {
        for (int slot = 0; slot < Summon::SkillCount(); ++slot)
        {
            if (Draw::MouseInRect(SkillRect(slot)))
                return slot;
        }
        return -1;
    }
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
    const wchar_t* hovered = nullptr;
    for (int slot = 0; slot < Summon::SkillCount(); ++slot)
    {
        RenderSkill(slot, control);
        if (Draw::MouseInRect(SkillRect(slot)))
            hovered = Summon::Skill(slot).name;
    }
    g_pRenderText->SetTextColor(backColor);
    if (hovered != nullptr)
        Draw::Tooltip(hovered);
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

    if (!Summon::Get().present)
        return true;

    const int skillSlot = SkillUnderMouse();
    if (skillSlot >= 0)
    {
        if (IsPress(VK_LBUTTON))
        {
            Draw::SwallowHeldClick();
            Summon::Cast(skillSlot, NO_TARGET);
        }
        return false;
    }

    if (!Draw::MouseInRect(Strip()))
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
