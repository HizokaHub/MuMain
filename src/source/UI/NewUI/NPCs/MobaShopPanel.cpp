#include "stdafx.h"
#include "UI/NewUI/NPCs/MobaShopPanel.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/NewUI3DRenderMng.h"
#include "UI/NewUI/Inventory/NewUIInventoryCtrl.h"
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "UI/Legacy/UIControls.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Engine/Object/ZzzInventory.h"
#include "Audio/DSPlaySound.h"
#include "GameLogic/Items/MobaShopPrices.h"

#include <algorithm>
#include <vector>

extern int MouseWheel;

namespace Draw = UI::Moba::Draw;
using Draw::Rect;
using SEASON3B::CNewUIInventoryCtrl;

namespace
{
    // Layout, in the 640x480 reference screen.
    constexpr int PANEL_X = 66;
    constexpr int PANEL_Y = 14;
    constexpr int COLUMNS = 6;
    constexpr int ROWS = 5;
    constexpr int CELL_W = 58;
    constexpr int CELL_H = 62;
    constexpr int CELL_GAP = 4;
    constexpr int PAD = 12;
    constexpr int HEADER_H = 48;
    constexpr int SUBTITLE_OFFSET_Y = 28;
    constexpr int TABS_H = 26;
    constexpr int FOOTER_H = 34;
    constexpr int CLOSE_SIZE = 18;
    constexpr int PANEL_W = (2 * PAD) + (COLUMNS * CELL_W) + ((COLUMNS - 1) * CELL_GAP);
    constexpr int GRID_H = (ROWS * CELL_H) + ((ROWS - 1) * CELL_GAP);
    constexpr int PANEL_H = HEADER_H + TABS_H + GRID_H + FOOTER_H + 8;

    constexpr int TAB_COUNT = 4;

    struct TierLook
    {
        float r, g, b;
        BYTE tr, tg, tb;
        const wchar_t* label;
    };

    constexpr TierLook TIER_LOOK[4] = {
        { 0.36f, 0.40f, 0.48f, 190, 195, 205, L"" },
        { 0.80f, 0.52f, 0.25f, 232, 164, 92, L"T1" },
        { 0.62f, 0.78f, 0.95f, 176, 214, 255, L"T2" },
        { 1.00f, 0.82f, 0.28f, 255, 214, 80, L"T3" },
    };

    int s_tab = 0;          // 0 = all, 1..3 = tier
    int s_scroll = 0;       // current page (a page is a full grid of cells)
    int s_hoverCell = -1;
    ITEM* s_hoverItem = nullptr;
    int s_hoverX = 0;
    int s_hoverY = 0;
    std::wstring s_pageTitle;

    Rect Panel() { return { PANEL_X, PANEL_Y, PANEL_W, PANEL_H }; }

    Rect CloseButton() { return { PANEL_X + PANEL_W - CLOSE_SIZE - 6, PANEL_Y + 7, CLOSE_SIZE, CLOSE_SIZE }; }

    Rect TabRect(int index)
    {
        const int w = 54;
        return { PANEL_X + PAD + (index * (w + 6)), PANEL_Y + HEADER_H + 2, w, TABS_H - 6 };
    }

    Rect GridOrigin() { return { PANEL_X + PAD, PANEL_Y + HEADER_H + TABS_H, PANEL_W - 2 * PAD, GRID_H }; }

    Rect CellRect(int slot)
    {
        const Rect grid = GridOrigin();
        const int column = slot % COLUMNS;
        const int row = slot / COLUMNS;
        return { grid.x + column * (CELL_W + CELL_GAP), grid.y + row * (CELL_H + CELL_GAP), CELL_W, CELL_H };
    }

    // The items shown with the current tab, in the order of the server.
    std::vector<ITEM*> Visible(CNewUIInventoryCtrl* store)
    {
        std::vector<ITEM*> items;
        if (store == nullptr)
            return items;

        const int count = store->GetNumberOfItems();
        for (int i = 0; i < count; ++i)
        {
            ITEM* item = store->GetItem(i);
            if (item == nullptr)
                continue;
            if (s_tab == 0 || GameLogic::Items::MobaShopPrices::TierOf(*item) == s_tab)
                items.push_back(item);
        }
        return items;
    }

    constexpr int PAGE_SIZE = COLUMNS * ROWS;

    int TotalPages(int itemCount) { return std::max(1, (itemCount + PAGE_SIZE - 1) / PAGE_SIZE); }

    // The last page (pages are counted from 0).
    int MaxScroll(int itemCount) { return TotalPages(itemCount) - 1; }

    Rect PrevPageButton() { return { PANEL_X + PANEL_W - PAD - 106, PANEL_Y + PANEL_H - 26, 24, 20 }; }

    Rect NextPageButton() { return { PANEL_X + PANEL_W - PAD - 24, PANEL_Y + PANEL_H - 26, 24, 20 }; }

    // The item in a visible cell (slot counted from the first visible row), or nullptr.
    ITEM* ItemAt(const std::vector<ITEM*>& items, int slot)
    {
        const int index = (s_scroll * PAGE_SIZE) + slot;
        return index >= 0 && index < static_cast<int>(items.size()) ? items[index] : nullptr;
    }

    void FitIcon(const ITEM& item, const Rect& cell, float& x, float& y, float& w, float& h)
    {
        const ITEM_ATTRIBUTE& attribute = ItemAttribute[item.Type];
        const float naturalW = static_cast<float>(std::max<int>(1, attribute.Width)) * 20.f;
        const float naturalH = static_cast<float>(std::max<int>(1, attribute.Height)) * 20.f;
        const float boxW = static_cast<float>(cell.w - 8);
        const float boxH = static_cast<float>(cell.h - 22);
        const float scale = std::min(boxW / naturalW, boxH / naturalH);
        w = naturalW * scale;
        h = naturalH * scale;
        x = static_cast<float>(cell.x) + (static_cast<float>(cell.w) - w) * 0.5f;
        y = static_cast<float>(cell.y) + 3.f + (boxH - h) * 0.5f;
    }

    void TooltipCallback(LPVOID, DWORD, DWORD)
    {
        if (s_hoverItem != nullptr)
            RenderItemInfo(s_hoverX, s_hoverY, s_hoverItem, true);
    }
}

namespace UI::Moba::ShopPanel
{

bool Active()
{
    return GameLogic::Items::MobaShopPrices::HasPrices();
}

void Reset()
{
    s_tab = 0;
    s_scroll = 0;
    s_hoverCell = -1;
    s_hoverItem = nullptr;
}

void SetPageTitle(const std::wstring& title)
{
    s_pageTitle = title;
}

void ClearPageTitle()
{
    s_pageTitle.clear();
}

bool Contains(int x, int y)
{
    return x >= PANEL_X && x < PANEL_X + PANEL_W && y >= PANEL_Y && y < PANEL_Y + PANEL_H;
}

MouseResult UpdateMouse(CNewUIInventoryCtrl* store, bool clicksEnabled)
{
    MouseResult result;
    s_hoverCell = -1;
    s_hoverItem = nullptr;
    if (!Active() || store == nullptr)
        return result;

    result.used = Contains(MouseX, MouseY);
    if (!result.used)
        return result;

    const std::vector<ITEM*> items = Visible(store);
    s_scroll = std::clamp(s_scroll, 0, MaxScroll(static_cast<int>(items.size())));
    if (MouseWheel != 0)
        s_scroll = std::clamp(s_scroll + (MouseWheel > 0 ? -1 : 1), 0, MaxScroll(static_cast<int>(items.size())));

    for (int slot = 0; slot < COLUMNS * ROWS; ++slot)
    {
        if (!Draw::MouseInRect(CellRect(slot)))
            continue;

        if (ITEM* item = ItemAt(items, slot))
        {
            s_hoverCell = slot;
            s_hoverItem = item;
            const Rect cell = CellRect(slot);
            s_hoverX = cell.x + cell.w;
            s_hoverY = cell.y + cell.h / 2;
        }
        break;
    }

    if (clicksEnabled && SEASON3B::IsRelease(VK_LBUTTON))
    {
        if (Draw::MouseInRect(CloseButton()))
        {
            result.close = true;
            return result;
        }

        if (Draw::MouseInRect(PrevPageButton()))
        {
            s_scroll = std::max(0, s_scroll - 1);
            PlayBuffer(SOUND_CLICK01);
            return result;
        }

        if (Draw::MouseInRect(NextPageButton()))
        {
            s_scroll = std::min(MaxScroll(static_cast<int>(items.size())), s_scroll + 1);
            PlayBuffer(SOUND_CLICK01);
            return result;
        }

        for (int tab = 0; tab < TAB_COUNT; ++tab)
        {
            if (Draw::MouseInRect(TabRect(tab)))
            {
                s_tab = tab;
                s_scroll = 0;
                PlayBuffer(SOUND_CLICK01);
                return result;
            }
        }

        if (s_hoverItem != nullptr && store != nullptr)
        {
            result.buyIndex = (s_hoverItem->y * store->GetNumberOfColumn()) + s_hoverItem->x;
            result.buyItem = s_hoverItem;
        }
    }

    return result;
}

void Render(CNewUIInventoryCtrl* store)
{
    if (!Active() || store == nullptr)
        return;

    const Rect panel = Panel();
    const std::vector<ITEM*> items = Visible(store);
    s_scroll = std::clamp(s_scroll, 0, MaxScroll(static_cast<int>(items.size())));

    // Frame and header.
    Draw::FillRect(panel, 0.035f, 0.045f, 0.075f, 0.96f);
    Draw::FrameRect(panel, 0.78f, 0.64f, 0.29f, 1.f);
    Draw::FillRect({ panel.x + 1, panel.y + 1, panel.w - 2, HEADER_H - 4 }, 0.08f, 0.10f, 0.16f, 1.f);

    // Tabs.
    static const wchar_t* const TAB_LABELS[TAB_COUNT] = { L"TODOS", L"T1", L"T2", L"T3" };
    for (int tab = 0; tab < TAB_COUNT; ++tab)
    {
        const Rect rect = TabRect(tab);
        const bool selected = tab == s_tab;
        const bool hover = Draw::MouseInRect(rect);
        const TierLook& look = TIER_LOOK[tab];
        Draw::FillRect(rect, selected ? look.r * 0.45f : 0.10f, selected ? look.g * 0.45f : 0.11f, selected ? look.b * 0.45f : 0.15f, 1.f);
        Draw::FrameRect(rect, look.r, look.g, look.b, selected || hover ? 1.f : 0.55f);
    }

    // Cells.
    const unsigned int gold = static_cast<unsigned int>(CharacterMachine->Gold);
    for (int slot = 0; slot < COLUMNS * ROWS; ++slot)
    {
        const Rect cell = CellRect(slot);
        ITEM* item = ItemAt(items, slot);
        if (item == nullptr)
        {
            Draw::FillRect(cell, 0.05f, 0.06f, 0.09f, 0.55f);
            Draw::FrameRect(cell, 0.14f, 0.16f, 0.22f, 1.f);
            continue;
        }

        const int tier = GameLogic::Items::MobaShopPrices::TierOf(*item);
        const TierLook& look = TIER_LOOK[tier];
        const bool hover = slot == s_hoverCell;
        Draw::FillRect(cell, hover ? 0.16f : 0.08f, hover ? 0.18f : 0.095f, hover ? 0.26f : 0.14f, 1.f);
        Draw::FrameRect(cell, look.r, look.g, look.b, hover ? 1.f : 0.7f);
    }
    EndRenderColor();

    // Texts (drawn after the colors so the font state is not mixed with the color batch).
    const HFONT body = Draw::Body();
    const HFONT bold = Draw::Bold();
    Draw::PutTextLeft(Draw::Title(), panel.x + PAD, panel.y + 6, 200, L"TIENDA", 255, 226, 150);
    if (!s_pageTitle.empty())
        Draw::PutTextLeft(body, panel.x + PAD, panel.y + SUBTITLE_OFFSET_Y, panel.w - 2 * PAD, s_pageTitle.c_str(), 200, 205, 215);
    wchar_t text[64] = {};
    swprintf(text, std::size(text), L"Oro: %u", gold);
    Draw::PutText(bold, panel.x + 150, panel.y + 9, panel.w - 150 - CLOSE_SIZE - 14, text, 255, 214, 80, RT3_SORT_RIGHT);
    Draw::PutTextCentered(bold, panel.x + panel.w - CLOSE_SIZE - 6, panel.y + 9, CLOSE_SIZE, L"X", 235, 235, 235);

    for (int tab = 0; tab < TAB_COUNT; ++tab)
    {
        const Rect rect = TabRect(tab);
        const TierLook& look = TIER_LOOK[tab];
        Draw::PutTextCentered(bold, rect.x, rect.y + 2, rect.w, TAB_LABELS[tab], look.tr, look.tg, look.tb);
    }

    for (int slot = 0; slot < COLUMNS * ROWS; ++slot)
    {
        ITEM* item = ItemAt(items, slot);
        if (item == nullptr)
            continue;

        const Rect cell = CellRect(slot);
        const int tier = GameLogic::Items::MobaShopPrices::TierOf(*item);
        const TierLook& look = TIER_LOOK[tier];
        if (tier > 0)
            Draw::PutTextLeft(body, cell.x + 3, cell.y + 1, 24, look.label, look.tr, look.tg, look.tb);
        if (item->Level > 0 && tier > 0)
        {
            swprintf(text, std::size(text), L"+%d", item->Level);
            Draw::PutText(body, cell.x + cell.w - 30, cell.y + 1, 27, text, 235, 235, 235, RT3_SORT_RIGHT);
        }

        uint32_t price = 0;
        if (GameLogic::Items::MobaShopPrices::TryGetBuyPrice(*item, price))
        {
            swprintf(text, std::size(text), L"%u", price);
            const bool affordable = gold >= price;
            Draw::PutTextCentered(bold, cell.x, cell.y + cell.h - 16, cell.w, text, affordable ? 120 : 255, affordable ? 235 : 110, affordable ? 130 : 110);
        }

        if (tier == 0 && item->Durability > 1)
        {
            swprintf(text, std::size(text), L"x%d", item->Durability);
            Draw::PutText(body, cell.x + cell.w - 30, cell.y + cell.h - 30, 27, text, 235, 235, 235, RT3_SORT_RIGHT);
        }
    }

    // Scroll hint and footer.
    const int pages = TotalPages(static_cast<int>(items.size()));
    {
        const Rect prev = PrevPageButton();
        const Rect next = NextPageButton();
        Draw::Button(prev, L"<", s_scroll > 0);
        Draw::Button(next, L"> ", s_scroll < pages - 1);
        swprintf(text, std::size(text), L"%d/%d", s_scroll + 1, pages);
        Draw::PutTextCentered(bold, prev.x + prev.w, prev.y + 3, next.x - prev.x - prev.w, text, 235, 235, 235);
    }

    Draw::PutTextLeft(body, panel.x + PAD, panel.y + panel.h - FOOTER_H + 2, panel.w - 2 * PAD - 120,
        L"Clic: comprar.  Vender: clic derecho en tu inventario.", 200, 205, 215);
}

void Render3D(CNewUIInventoryCtrl* store)
{
    if (!Active() || store == nullptr)
        return;

    const std::vector<ITEM*> items = Visible(store);
    for (int slot = 0; slot < COLUMNS * ROWS; ++slot)
    {
        ITEM* item = ItemAt(items, slot);
        if (item == nullptr)
            continue;

        float x, y, w, h;
        FitIcon(*item, CellRect(slot), x, y, w, h);
        glColor4f(1.f, 1.f, 1.f, 1.f);
        RenderItem3D(x, y, w, h, item->Type, item->Level, item->ExcellentFlags, item->AncientDiscriminator, false);
    }
}

void QueueTooltip()
{
    if (s_hoverItem != nullptr && g_pNewUI3DRenderMng != nullptr)
        g_pNewUI3DRenderMng->RenderUI2DEffect(INVENTORY_CAMERA_Z_ORDER, TooltipCallback, nullptr, 0, 0);
}

}
