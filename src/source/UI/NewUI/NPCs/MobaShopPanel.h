#pragma once

#include <string>

namespace SEASON3B
{
    class CNewUIInventoryCtrl;
}

struct tagITEM;

// The panel of the MOBA item shop: a grid of cells with the icon, the price (green when the champion can pay it, red when not), the tier and the level of
// every item of the vendor, tabs to filter the tiers and a scroll, instead of the native window of the vendor. It does not own any data: the items are the ones
// the server put in the grid of the native window (CNewUINPCShop), in the order of the server (T1 to T3), and the prices are the ones the server pushed
// (GameLogic::Items::MobaShopPrices). A click buys, a stack or a worn item of the inventory is sold with the right click or by dropping it on the panel.
namespace UI::Moba::ShopPanel
{
    struct MouseResult
    {
        int buyIndex = -1;     // grid index (row * columns + column) of the item to buy, or -1
        tagITEM* buyItem = nullptr;  // that item
        bool close = false;    // the close button was clicked
        bool used = false;     // the mouse is over the panel (the world must not get the click)
    };

    // Whether the MOBA shop is open (the server sent its prices): the panel replaces the native window.
    bool Active();

    // Forgets the tab and the scroll (the shop opened or closed).
    void Reset();

    // The name of the vendor menu page being shown (the option the player picked), drawn under the title. It is set before the shop opens, so Reset() keeps
    // it; ClearPageTitle() drops it when the shop closes.
    void SetPageTitle(const std::wstring& title);
    void ClearPageTitle();

    // Whether the point is over the panel.
    bool Contains(int x, int y);

    // Mouse handling; <clicksEnabled> is false for the click that opened the shop.
    MouseResult UpdateMouse(SEASON3B::CNewUIInventoryCtrl* store, bool clicksEnabled);

    // The frame, the tabs and the cells (2D).
    void Render(SEASON3B::CNewUIInventoryCtrl* store);

    // The icons of the items (3D pass).
    void Render3D(SEASON3B::CNewUIInventoryCtrl* store);

    // Queues the tooltip of the item under the mouse (call every frame while the shop is open).
    void QueueTooltip();
}
