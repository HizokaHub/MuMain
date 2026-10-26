#include "stdafx.h"
#include "World/Moba/MobaPuppet.h"

#include "Network/Server/WSclient.h"       // SendMobaPuppet*
#include "UI/NewUI/NewUICommon.h"          // IsPress / IsRepeat / IsRelease
#include "Render/Textures/ZzzOpenglUtil.h" // the mouse buttons
#include "Engine/Object/ZzzInterface.h"    // MobaPuppetCastAtCursor

#include <algorithm>

extern double WorldTime;

namespace World::Moba::Puppet
{

namespace
{
    // Without news for this long the panel is taken as gone (the match ended or the packets stopped).
    constexpr double STALE_AFTER_MS = 4000.0;
    constexpr unsigned char ACTION_RELEASE = 0;
    constexpr unsigned char ACTION_SELECT = 1;
    constexpr int FIRST_PRINTABLE_KEY = '0';
    constexpr int LAST_PRINTABLE_KEY = 'Z';

    State s_state;
    double s_updatedMs = -1.0e9;
    int s_slotSkill[SLOT_COUNT] = {};
    int s_rebindPending = 0;
    bool s_ctrlDown = false;
    bool s_ctrlDirty = false;

    // The slots hold the skills in the order the server sent them (the owner's picks, then the buffs); a new list keeps the keys the player changed.
    void RebuildSlots()
    {
        for (int& skill : s_slotSkill)
        {
            if (skill != 0 && FindSkill(skill) == nullptr)
                skill = 0;
        }

        for (const Skill& skill : s_state.skills)
        {
            const bool bound = std::find(std::begin(s_slotSkill), std::end(s_slotSkill), skill.number) != std::end(s_slotSkill);
            if (bound)
                continue;

            int* free = std::find(std::begin(s_slotSkill), std::end(s_slotSkill), 0);
            if (free != std::end(s_slotSkill))
                *free = skill.number;
        }
    }

    // Whether anything else was pressed while Ctrl is held (a click or a key): then Ctrl was a modifier, not a tap.
    bool AnyOtherInput()
    {
        if (MouseLButton || MouseRButton || MouseLButtonPush || MouseRButtonPush)
            return true;

        for (int key = FIRST_PRINTABLE_KEY; key <= LAST_PRINTABLE_KEY; ++key)
        {
            if (SEASON3B::IsPress(key))
                return true;
        }
        return SEASON3B::IsPress(VK_TAB) || SEASON3B::IsPress(VK_SPACE) || SEASON3B::IsPress(VK_SHIFT);
    }

    void UpdateControlTap()
    {
        if (SEASON3B::IsPress(VK_CONTROL))
        {
            s_ctrlDown = true;
            s_ctrlDirty = false;
        }
        else if (s_ctrlDown && SEASON3B::IsRepeat(VK_CONTROL))
        {
            s_ctrlDirty = s_ctrlDirty || AnyOtherInput();
        }
        else if (s_ctrlDown)
        {
            s_ctrlDown = false;
            if (!s_ctrlDirty)
                ToggleControl();
        }
    }
}

void Set(State state)
{
    s_state = std::move(state);
    s_updatedMs = WorldTime;
    if (!s_state.controlling)
        s_rebindPending = 0;
    RebuildSlots();
}

const State& Get()
{
    static const State s_none;
    return (WorldTime - s_updatedMs > STALE_AFTER_MS) ? s_none : s_state;
}

bool Controlling()
{
    return Get().controlling;
}

const Candidate* Controlled()
{
    for (const Candidate& candidate : Get().candidates)
    {
        if (candidate.selected)
            return &candidate;
    }
    return nullptr;
}

void ToggleControl()
{
    const State& state = Get();
    if (state.controlling)
    {
        SendMobaPuppetSelect(ACTION_RELEASE, 0);
        return;
    }

    if (!state.candidates.empty())
        SendMobaPuppetSelect(ACTION_SELECT, state.candidates.front().key);
}

void Select(unsigned short key)
{
    SendMobaPuppetSelect(ACTION_SELECT, key);
}

void OrderMove(int tileX, int tileY)
{
    if (!Controlling() || tileX < 0 || tileX > 255 || tileY < 0 || tileY > 255)
        return;

    SendMobaPuppetOrder(1, static_cast<unsigned char>(tileX), static_cast<unsigned char>(tileY), 0);
}

void OrderAttack(unsigned short key)
{
    if (!Controlling())
        return;

    SendMobaPuppetOrder(2, 0, 0, key);
}

void Cast(int skillNumber, unsigned short key, int tileX, int tileY)
{
    if (!Controlling() || skillNumber <= 0)
        return;

    const unsigned char x = static_cast<unsigned char>(std::clamp(tileX, 0, 255));
    const unsigned char y = static_cast<unsigned char>(std::clamp(tileY, 0, 255));
    SendMobaPuppetCast(static_cast<unsigned short>(skillNumber), key, x, y);
}

void Recall()
{
    if (Controlling())
        SendMobaPuppetRecall();
}

int SkillOfSlot(int slot)
{
    return (slot >= 0 && slot < SLOT_COUNT) ? s_slotSkill[slot] : 0;
}

void Rebind(int skillNumber, int slot)
{
    s_rebindPending = 0;
    if (slot < 0 || slot >= SLOT_COUNT || FindSkill(skillNumber) == nullptr)
        return;

    int* current = std::find(std::begin(s_slotSkill), std::end(s_slotSkill), skillNumber);
    if (current != std::end(s_slotSkill))
        *current = s_slotSkill[slot];
    s_slotSkill[slot] = skillNumber;
}

int RebindPending()
{
    return s_rebindPending;
}

void SetRebindPending(int skillNumber)
{
    s_rebindPending = (s_rebindPending == skillNumber) ? 0 : skillNumber;
}

const Skill* FindSkill(int skillNumber)
{
    for (const Skill& skill : s_state.skills)
    {
        if (skill.number == skillNumber)
            return &skill;
    }
    return nullptr;
}

bool UpdateKeys()
{
    UpdateControlTap();
    if (!Controlling())
        return false;

    for (int slot = 0; slot < SLOT_COUNT; ++slot)
    {
        if (!SEASON3B::IsPress('1' + slot))
            continue;

        if (s_rebindPending != 0)
            Rebind(s_rebindPending, slot);
        else
            MobaPuppetCastAtCursor(s_slotSkill[slot]);
        return true;
    }
    return false;
}

}
