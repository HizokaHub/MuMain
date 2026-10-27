#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaPetHud.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaExtraPets.h"
#include "World/MapInfra/MapManager.h"
#include "Engine/Object/ZzzCharacter.h"

using namespace SEASON3B;
namespace ExtraPets = World::Moba::ExtraPets;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen: centered over the skill bar (slots at x = 222 + 32 * i, i = 0..10, from y = 431), right above it.
    constexpr int BAR_CENTER_X = 222 + 32 * 11 / 2;
    constexpr int BAR_Y = 431;
    constexpr int BADGE_SIZE = 22;
    constexpr int BADGE_GAP = 3;
    constexpr int ROW_Y = BAR_Y - BADGE_SIZE - 3;
    constexpr int RESERVED_HEIGHT = BADGE_SIZE + 3 + 2;

    constexpr float LAYER_DEPTH = 10.2f;
    constexpr float KEY_EVENT_ORDER = 50.0f;
    constexpr DWORD UI_KEY_OFFSET = 11;

    struct PetLook
    {
        const wchar_t* name;
        const wchar_t* tag;
        float r, g, b;
    };

    // In the order of the bits of the mask (see MobaExtraPets.h).
    constexpr PetLook PET_LOOK[ExtraPets::PET_COUNT] = {
        { L"Guardian Angel", L"An", 0.95f, 0.85f, 0.45f },
        { L"Imp", L"Im", 0.85f, 0.30f, 0.25f },
        { L"Demon", L"De", 0.60f, 0.20f, 0.65f },
        { L"Spirit of Guardian", L"Sp", 0.35f, 0.70f, 0.90f },
        { L"Rudolf", L"Ru", 0.80f, 0.20f, 0.30f },
        { L"Panda", L"Pa", 0.70f, 0.70f, 0.72f },
        { L"Unicornio", L"Un", 0.90f, 0.60f, 0.85f },
        { L"Skeleton", L"Sk", 0.78f, 0.78f, 0.60f },
        { L"Dark Raven", L"Cu", 0.35f, 0.35f, 0.55f },
    };

    int CountOf(unsigned short mask)
    {
        int count = 0;
        for (int bit = 0; bit < ExtraPets::PET_COUNT; ++bit)
            count += (mask & (1 << bit)) != 0 ? 1 : 0;
        return count;
    }

    // The slot-th badge of a row of <count> badges, centered on the bar.
    Rect BadgeRect(int slot, int count)
    {
        const int rowWidth = count * BADGE_SIZE + (count - 1) * BADGE_GAP;
        return { BAR_CENTER_X - rowWidth / 2 + slot * (BADGE_SIZE + BADGE_GAP), ROW_Y, BADGE_SIZE, BADGE_SIZE };
    }

    unsigned short OwnMask()
    {
        if (Hero == nullptr || gMapManager.WorldActive != WD_200_MOBA_ARENA)
            return 0;
        return ExtraPets::MaskOf(static_cast<unsigned short>(Hero->Key));
    }

    bool IsOn(unsigned short mask, int bit)
    {
        return (mask & (1 << bit)) != 0;
    }
}

void CMobaPetHud::Create(CNewUIManager* pNewUIMng)
{
    static CMobaPetHud s_hud;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_hud);
}

int CMobaPetHud::ReservedHeight()
{
    return OwnMask() != 0 ? RESERVED_HEIGHT : 0;
}

bool CMobaPetHud::Render()
{
    const unsigned short mask = OwnMask();
    if (mask == 0)
        return true;

    const int count = CountOf(mask);
    int slot = 0;
    for (int bit = 0; bit < ExtraPets::PET_COUNT; ++bit)
    {
        if (!IsOn(mask, bit))
            continue;

        const PetLook& look = PET_LOOK[bit];
        const Rect badge = BadgeRect(slot++, count);
        Draw::FillRect(badge, look.r * 0.35f, look.g * 0.35f, look.b * 0.35f, 1.f);
        Draw::FrameRect(badge, look.r, look.g, look.b, 1.f);
    }
    EndRenderColor();

    const DWORD backColor = g_pRenderText->GetTextColor();
    const wchar_t* hoveredName = nullptr;
    slot = 0;
    for (int bit = 0; bit < ExtraPets::PET_COUNT; ++bit)
    {
        if (!IsOn(mask, bit))
            continue;

        const PetLook& look = PET_LOOK[bit];
        const Rect badge = BadgeRect(slot++, count);
        Draw::PutTextCentered(Draw::Bold(), badge.x, badge.y + 5, badge.w, look.tag, 240, 240, 240);
        if (Draw::MouseInRect(badge))
            hoveredName = look.name;
    }
    g_pRenderText->SetTextColor(backColor);

    if (hoveredName != nullptr)
        Draw::Tooltip(hoveredName);
    return true;
}

bool CMobaPetHud::Update()
{
    return true;
}

bool CMobaPetHud::UpdateMouseEvent()
{
    return true;
}

bool CMobaPetHud::UpdateKeyEvent()
{
    return true;
}

float CMobaPetHud::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaPetHud::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
