#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaPuppetHud.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor, WorldTime
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "UI/NewUI/HUD/Skills/SkillIcon.h"
#include "World/Moba/MobaPuppet.h"

#include <algorithm>

extern double WorldTime;

using namespace SEASON3B;
namespace Puppet = World::Moba::Puppet;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen. The cards sit on the left side; the skill bar above the strip of the Elf's creature.
    constexpr int PANEL_X = 8;
    constexpr int PANEL_Y = 206;
    constexpr int HEADER_H = 14;
    constexpr int CARD_W = 172;
    constexpr int CARD_H = 44;
    constexpr int CARD_GAP = 4;
    constexpr int PORTRAIT_SIZE = 36;
    constexpr int CARD_PAD = 4;
    constexpr int NAME_X = 44;
    constexpr int BAR_W = 120;
    constexpr int BAR_H = 7;
    constexpr int HP_BAR_Y = 19;
    constexpr int MP_BAR_Y = 30;

    constexpr int SLOT_SIZE = 34;
    constexpr int SLOT_GAP = 3;
    constexpr int ICON_INSET = 3;
    constexpr int BAR_Y = 356;
    constexpr int HINT_Y = 342;
    constexpr int SCREEN_W = 640;
    constexpr int KEY_LABEL_X = 3;
    constexpr int KEY_LABEL_Y = 2;
    constexpr int RANK_LABEL_Y = 22;
    constexpr int SECONDS_Y = 11;
    constexpr int MS_PER_SECOND = 1000;

    constexpr float LAYER_DEPTH = 10.3f;
    constexpr float KEY_EVENT_ORDER = 51.0f;
    constexpr DWORD UI_KEY_OFFSET = 10;

    constexpr float COOLDOWN_SHADE = 0.62f;
    constexpr float INACTIVE_SHADE = 0.68f;
    constexpr int LOW_HP_PERCENT = 35;

    bool s_portraitsLoaded = false;

    Rect CardRect(int index)
    {
        return { PANEL_X, PANEL_Y + HEADER_H + index * (CARD_H + CARD_GAP), CARD_W, CARD_H };
    }

    int SlotCount()
    {
        int count = 0;
        for (int slot = 0; slot < Puppet::SLOT_COUNT; ++slot)
        {
            if (Puppet::SkillOfSlot(slot) != 0)
                ++count;
        }
        return count;
    }

    // The bar is centred; empty slots are skipped, so the slot index and the screen position differ.
    Rect SlotRect(int position, int count)
    {
        const int total = count * SLOT_SIZE + (count - 1) * SLOT_GAP;
        return { (SCREEN_W - total) / 2 + position * (SLOT_SIZE + SLOT_GAP), BAR_Y, SLOT_SIZE, SLOT_SIZE };
    }

    void RenderBar(const Rect& area, int percent, float red, float green, float blue)
    {
        Draw::FillRect(area, 0.10f, 0.11f, 0.14f, 1.f);
        Draw::FillRect({ area.x, area.y, area.w * std::clamp(percent, 0, 100) / 100, area.h }, red, green, blue, 1.f);
        Draw::FrameRect(area, 0.23f, 0.27f, 0.35f, 1.f);
    }

    void RenderCard(const Rect& card, const Puppet::Candidate& candidate)
    {
        const bool hot = Draw::MouseInRect(card);
        Draw::FillRect(card, 0.04f, 0.055f, 0.086f, 0.92f);
        Draw::FrameRect(card, candidate.selected ? 0.95f : 0.78f, candidate.selected ? 0.80f : 0.64f, candidate.selected ? 0.40f : 0.29f, hot ? 1.f : 0.8f);
        EndRenderColor();

        Draw::Portrait({ card.x + CARD_PAD, card.y + CARD_PAD, PORTRAIT_SIZE, PORTRAIT_SIZE }, candidate.baseClass, true);
        Draw::PutTextLeft(Draw::Bold(), card.x + NAME_X, card.y + 3, BAR_W + 20, candidate.name.c_str(), 233, 228, 212);
        RenderBar({ card.x + NAME_X, card.y + HP_BAR_Y, BAR_W, BAR_H }, candidate.hpPercent,
            candidate.hpPercent > LOW_HP_PERCENT ? 0.20f : 0.78f, candidate.hpPercent > LOW_HP_PERCENT ? 0.70f : 0.22f, 0.25f);
        RenderBar({ card.x + NAME_X, card.y + MP_BAR_Y, BAR_W, BAR_H }, candidate.mpPercent, 0.20f, 0.42f, 0.90f);
        EndRenderColor();
    }

    void RenderCards()
    {
        const Puppet::State& state = Puppet::Get();
        if (state.candidates.empty())
            return;

        Draw::PutTextLeft(Draw::Bold(), PANEL_X, PANEL_Y, CARD_W, state.controlling ? L"Controlando (Ctrl: soltar)" : L"Sin jugador (Ctrl: controlar)", 255, 220, 90);
        for (size_t i = 0; i < state.candidates.size(); ++i)
            RenderCard(CardRect(static_cast<int>(i)), state.candidates[i]);
    }

    void RenderCooldown(const Rect& slot, const Puppet::Skill& skill)
    {
        const double remainMs = skill.cooldownEndMs - WorldTime;
        if (remainMs <= 0.0)
            return;

        const double fullMs = std::max(skill.cooldownFullMs, remainMs);
        const float fraction = static_cast<float>(std::clamp(remainMs / fullMs, 0.0, 1.0));
        const int inner = SLOT_SIZE - 2 * ICON_INSET;
        const int sweep = static_cast<int>(static_cast<float>(inner) * fraction);
        Draw::FillRect({ slot.x + ICON_INSET, slot.y + ICON_INSET + (inner - sweep), inner, sweep }, 0.f, 0.f, 0.f, COOLDOWN_SHADE);
        EndRenderColor();

        wchar_t seconds[16] = {};
        swprintf(seconds, std::size(seconds), L"%d", static_cast<int>(remainMs / MS_PER_SECOND) + 1);
        Draw::PutTextCentered(Draw::Bold(), slot.x, slot.y + SECONDS_Y, SLOT_SIZE, seconds, 255, 255, 230);
    }

    void RenderSlot(const Rect& slot, int key, const Puppet::Skill& skill)
    {
        const bool waiting = Puppet::RebindPending() == skill.number;
        Draw::FillRect(slot, 0.04f, 0.055f, 0.086f, 1.f);
        Draw::FrameRect(slot, waiting ? 1.f : 0.78f, waiting ? 0.9f : 0.64f, waiting ? 0.2f : 0.29f, 1.f);
        EndRenderColor();
        UI::Skills::Icon::Render(skill.number, static_cast<float>(slot.x + ICON_INSET), static_cast<float>(slot.y + ICON_INSET),
            static_cast<float>(SLOT_SIZE - 2 * ICON_INSET), static_cast<float>(SLOT_SIZE - 2 * ICON_INSET));

        if (skill.rank == 0)
        {
            Draw::FillRect({ slot.x + ICON_INSET, slot.y + ICON_INSET, SLOT_SIZE - 2 * ICON_INSET, SLOT_SIZE - 2 * ICON_INSET }, 0.f, 0.f, 0.f, INACTIVE_SHADE);
            EndRenderColor();
        }

        RenderCooldown(slot, skill);

        wchar_t label[8] = {};
        swprintf(label, std::size(label), L"%d", key);
        Draw::PutTextLeft(Draw::Bold(), slot.x + KEY_LABEL_X, slot.y + KEY_LABEL_Y, SLOT_SIZE, label, 255, 255, 255);
        swprintf(label, std::size(label), L"%d", skill.rank);
        Draw::PutTextLeft(Draw::Bold(), slot.x + KEY_LABEL_X, slot.y + RANK_LABEL_Y, SLOT_SIZE, label, 255, 217, 51);
    }

    void RenderSkillBar()
    {
        const Puppet::Candidate* controlled = Puppet::Controlled();
        const int count = SlotCount();
        if (!Puppet::Controlling() || controlled == nullptr || count == 0)
            return;

        const wchar_t* hint = Puppet::RebindPending() != 0 ? L"Pulsa una tecla 1-9 para esa skill"
            : L"Clic derecho: mover / atacar    1-9: skills    B: volver a base    Ctrl: soltar";
        wchar_t title[96] = {};
        swprintf(title, std::size(title), L"%ls   %ls", controlled->name.c_str(), hint);
        Draw::PutTextCentered(Draw::Bold(), 0, HINT_Y, SCREEN_W, title, 255, 220, 90);

        int position = 0;
        for (int slot = 0; slot < Puppet::SLOT_COUNT; ++slot)
        {
            const Puppet::Skill* skill = Puppet::FindSkill(Puppet::SkillOfSlot(slot));
            if (skill != nullptr)
                RenderSlot(SlotRect(position++, count), slot + 1, *skill);
        }
    }

    // The card under the mouse, or -1.
    int CardUnderMouse()
    {
        const Puppet::State& state = Puppet::Get();
        for (size_t i = 0; i < state.candidates.size(); ++i)
        {
            if (Draw::MouseInRect(CardRect(static_cast<int>(i))))
                return static_cast<int>(i);
        }
        return -1;
    }

    // The skill under the mouse, or 0.
    int SlotUnderMouse()
    {
        const int count = SlotCount();
        int position = 0;
        for (int slot = 0; slot < Puppet::SLOT_COUNT && Puppet::Controlling(); ++slot)
        {
            const int skill = Puppet::SkillOfSlot(slot);
            if (skill == 0)
                continue;

            if (Draw::MouseInRect(SlotRect(position++, count)))
                return skill;
        }
        return 0;
    }
}

void CMobaPuppetHud::Create(CNewUIManager* pNewUIMng)
{
    static CMobaPuppetHud s_hud;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_hud);
}

bool CMobaPuppetHud::Render()
{
    const Puppet::State& state = Puppet::Get();
    if (state.candidates.empty())
    {
        s_portraitsLoaded = false;
        return true;
    }

    if (!s_portraitsLoaded)
    {
        Draw::LoadPortraits();
        s_portraitsLoaded = true;
    }

    const DWORD backColor = g_pRenderText->GetTextColor();
    RenderCards();
    RenderSkillBar();
    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaPuppetHud::Update()
{
    return true;
}

bool CMobaPuppetHud::UpdateMouseEvent()
{
    if (Draw::HeldClickSwallowed())
        return false;

    const int card = CardUnderMouse();
    const int skill = SlotUnderMouse();
    if (card < 0 && skill == 0)
        return true;

    if (IsPress(VK_LBUTTON))
    {
        Draw::SwallowHeldClick();
        if (card >= 0)
            Puppet::Select(Puppet::Get().candidates[card].key);
        else
            Puppet::SetRebindPending(skill);
    }

    // The cards and the bar swallow the mouse: a click on them must not walk the hero.
    return false;
}

bool CMobaPuppetHud::UpdateKeyEvent()
{
    return true;
}

float CMobaPuppetHud::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaPuppetHud::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
