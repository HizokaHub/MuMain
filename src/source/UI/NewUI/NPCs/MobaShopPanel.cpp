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
#include "Network/Server/WSclient.h"          // SendMobaNpcMenuSelect
#include "GameLogic/Items/MobaShopPrices.h"

#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>

extern int MouseWheel;

namespace Draw = UI::Moba::Draw;
using Draw::Rect;
using SEASON3B::CNewUIInventoryCtrl;
namespace Prices = GameLogic::Items::MobaShopPrices;

namespace
{
    // Layout, in the 640x480 reference screen. The panel keeps to the left 444 px: the inventory window of the player opens on the right (from x = 450) while
    // the shop is open and must stay visible (items are sold by dropping them, the perfection jewel is dragged onto them). Tabs across the top, then the
    // recommended build (left) and the catalog (middle), and below them the strip with the detail of an item, its stats if bought and the buttons.
    constexpr int PANEL_X = 4;
    constexpr int PANEL_Y = 4;
    constexpr int PANEL_W = 444;
    constexpr int PANEL_H = 420;
    constexpr int PAD = 6;
    constexpr int HEADER_H = 28;
    constexpr int TAB_H = 28;
    constexpr int TAB_GAP = 3;
    constexpr int CONTENT_Y = PANEL_Y + HEADER_H + 4 + TAB_H + 6;
    constexpr int CONTENT_BOTTOM = PANEL_Y + PANEL_H - 20;
    constexpr int STRIP_H = 82;
    constexpr int STRIP_Y = CONTENT_BOTTOM - STRIP_H;
    constexpr int CLOSE_SIZE = 18;

    constexpr int LEFT_X = PANEL_X + PAD;
    constexpr int LEFT_W = 104;
    constexpr int CENTER_X = LEFT_X + LEFT_W + 6;
    constexpr int CENTER_W = PANEL_X + PANEL_W - PAD - CENTER_X;

    // The recommended build (left column).
    constexpr int STEP_TITLE_H = 18;
    constexpr int STEP_ROW_H = 38;
    constexpr int STEP_ICON = 30;
    constexpr int STEP_NAME_CHARS = 11;

    // The grid of the pages which are not sets.
    constexpr int GRID_COLUMNS = 6;
    constexpr int GRID_ROWS = 4;
    constexpr int GRID_CELL_W = 52;
    constexpr int GRID_CELL_H = 48;
    constexpr int GRID_GAP = 3;
    constexpr int PAGE_SIZE = GRID_COLUMNS * GRID_ROWS;

    // The sets: one column per set, its five pieces from top to bottom.
    constexpr int SET_COLUMNS_MAX = 5;
    constexpr int SET_COL_W = 63;
    constexpr int SET_COL_GAP = 2;
    constexpr int SET_HEADER_H = 26;
    constexpr int SET_CELL_H = 36;
    constexpr int SET_BUY_H = 18;
    constexpr int PIECES_PER_SET = 5;

    constexpr int CLIENT_GROUP_SIZE = 512;
    constexpr int FIRST_PIECE_GROUP = 7;
    constexpr int ARMOR_GROUP = 8;
    // Helm, armor, gloves, pants, boots (the order the player reads a set).
    constexpr int PIECE_ORDER[PIECES_PER_SET] = { 7, 8, 10, 9, 11 };
    constexpr int SERVER_MENU_ID = 1;

    // The strip of the detail (below the build and the catalog).
    constexpr int STRIP_ICON = 56;
    constexpr int STRIP_INFO_X = 66;
    constexpr int STRIP_PREVIEW_X = 192;
    constexpr int STRIP_BUTTON_W = 96;
    constexpr int STRIP_BUTTON_H = 24;
    constexpr int STRIP_SMALL_H = 22;
    constexpr int PREVIEW_MAX_LINES = 4;
    constexpr int INFO_LINE_H = 14;
    constexpr DWORD PREVIEW_TIMEOUT_MS = 2500;

    enum Tab : int
    {
        TAB_FOR_YOU,
        TAB_CONSUMABLES,
        TAB_WEAPONS,
        TAB_SETS_T1,
        TAB_SETS_T2,
        TAB_SETS_T3,
        TAB_PETS,
        TAB_COUNT
    };
    constexpr int FIRST_SETS_TAB = TAB_SETS_T1;
    constexpr int LAST_SETS_TAB = TAB_SETS_T3;

    const wchar_t* const TAB_LABEL[TAB_COUNT] = {
        L"PARA TI", L"CONSUM.", L"ARMAS", L"SETS T1", L"SETS T2", L"SETS T3", L"MASCOTAS",
    };
    const wchar_t* const TAB_LABEL_SMALL[TAB_COUNT] = {
        L"", L"y acces.", L"", L"", L"", L"", L"y escudos",
    };

    const wchar_t* const ROLE_LABEL[] = { L"Equilibrado", L"Ofensivo", L"Recursos", L"Tanque", L"Utilidad" };
    constexpr int ROLE_COUNT = 5;

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

    struct Cell
    {
        Rect rect;
        ITEM* item;
    };

    struct SetColumn
    {
        Rect rect;
        Rect buyButton;
        uint16_t armorType = 0;
        bool recommended = false;
        std::vector<Cell> pieces;
        uint32_t price = 0;
    };

    int s_tab = TAB_FOR_YOU;
    int s_scroll = 0;
    ITEM* s_hoverItem = nullptr;
    ITEM* s_selected = nullptr;
    ITEM* s_previewAskedFor = nullptr;       // the item whose preview was requested
    int s_previewSlotAsked = -1;             // its slot in the window of the shop
    DWORD s_previewAskedAt = 0;              // when (GetTickCount)
    int s_hoverX = 0;
    int s_hoverY = 0;
    std::vector<Cell> s_cells;               // the grid or the pieces of the sets, as laid out this frame
    std::vector<SetColumn> s_columns;
    std::vector<ITEM*> s_buyQueue;           // the pieces of a set being bought, one at a time
    std::wstring s_pageTitle;

    Rect Panel() { return { PANEL_X, PANEL_Y, PANEL_W, PANEL_H }; }

    Rect CloseButton() { return { PANEL_X + PANEL_W - CLOSE_SIZE - 6, PANEL_Y + 5, CLOSE_SIZE, CLOSE_SIZE }; }

    Rect TabRect(int tab)
    {
        const int width = (PANEL_W - 2 * PAD - (TAB_COUNT - 1) * TAB_GAP) / TAB_COUNT;
        return { PANEL_X + PAD + tab * (width + TAB_GAP), PANEL_Y + HEADER_H + 4, width, TAB_H };
    }

    Rect StepRect(int index) { return { LEFT_X, CONTENT_Y + STEP_TITLE_H + index * STEP_ROW_H, LEFT_W, STEP_ROW_H - 4 }; }

    Rect Strip() { return { PANEL_X + PAD, STRIP_Y, PANEL_W - 2 * PAD, STRIP_H }; }

    Rect StripIcon() { return { PANEL_X + PAD + 4, STRIP_Y + 12, STRIP_ICON, STRIP_ICON }; }

    Rect StripButtonsX() { return { PANEL_X + PANEL_W - PAD - STRIP_BUTTON_W - 4, STRIP_Y, STRIP_BUTTON_W, STRIP_H }; }

    Rect DetailBuyButton() { return { StripButtonsX().x, STRIP_Y + 6, STRIP_BUTTON_W, STRIP_BUTTON_H }; }

    // Sell (the copy of the item the champion carries) and undo (the last purchase), below the buy button.
    Rect SellButton() { return { StripButtonsX().x, STRIP_Y + 6 + STRIP_BUTTON_H + 4, STRIP_BUTTON_W, STRIP_SMALL_H }; }

    Rect UndoButton() { return { StripButtonsX().x, STRIP_Y + 6 + STRIP_BUTTON_H + 4 + STRIP_SMALL_H + 4, STRIP_BUTTON_W, STRIP_SMALL_H }; }

    Rect PrevPageButton() { return { CENTER_X + CENTER_W - 106, STRIP_Y - 24, 24, 18 }; }

    Rect NextPageButton() { return { CENTER_X + CENTER_W - 24, STRIP_Y - 24, 24, 18 }; }

    bool IsSetsTab() { return s_tab >= FIRST_SETS_TAB && s_tab <= LAST_SETS_TAB; }

    bool IsGridTab() { return s_tab != TAB_FOR_YOU && !IsSetsTab(); }

    int TabOfPage(int page) { return page + 1; }

    int Group(const ITEM& item) { return item.Type / CLIENT_GROUP_SIZE; }

    int Number(const ITEM& item) { return item.Type % CLIENT_GROUP_SIZE; }

    int TierOfType(uint16_t type, int fallback)
    {
        for (const Prices::Recommended& step : Prices::GetRecommended())
        {
            if (step.Type == type)
                return step.Tier;
        }
        return fallback;
    }

    // The name of a set from its armor piece: "Dragon Armor" -> "Dragon".
    std::wstring SetNameOf(uint16_t armorType)
    {
        std::wstring name = ItemAttribute[armorType].Name;
        const size_t space = name.find_last_of(L' ');
        if (space != std::wstring::npos)
        {
            const std::wstring last = name.substr(space + 1);
            if (last == L"Armor" || last == L"Armour" || last == L"Helm" || last == L"Pants" || last == L"Gloves" || last == L"Boots")
                name.erase(space);
        }
        return name;
    }

    // Whether the champion has an item of that type (worn or in the inventory).
    bool Owns(uint16_t type)
    {
        for (int slot = 0; slot < MAX_EQUIPMENT; ++slot)
        {
            if (CharacterMachine->Equipment[slot].Type == type)
                return true;
        }

        CNewUIInventoryCtrl* inventory = g_pMyInventory != nullptr ? g_pMyInventory->GetInventoryCtrl() : nullptr;
        if (inventory == nullptr)
            return false;

        for (int i = 0; i < inventory->GetNumberOfItems(); ++i)
        {
            ITEM* item = inventory->GetItem(i);
            if (item != nullptr && item->Type == type)
                return true;
        }
        return false;
    }

    // The first step of the recommended build the champion does not have yet, or -1.
    int NextStep()
    {
        const std::vector<Prices::Recommended>& steps = Prices::GetRecommended();
        for (size_t i = 0; i < steps.size(); ++i)
        {
            if (!Owns(steps[i].Type))
                return static_cast<int>(i);
        }
        return -1;
    }

    // A name cut to <maxChars> characters (a dot marks the cut).
    std::wstring Shorten(const std::wstring& name, size_t maxChars)
    {
        return name.size() <= maxChars ? name : name.substr(0, maxChars - 1) + L".";
    }

    std::vector<ITEM*> StoreItems(CNewUIInventoryCtrl* store)
    {
        std::vector<ITEM*> items;
        if (store == nullptr)
            return items;

        const int count = store->GetNumberOfItems();
        for (int i = 0; i < count; ++i)
        {
            if (ITEM* item = store->GetItem(i))
                items.push_back(item);
        }
        return items;
    }

    int TotalPages(int itemCount) { return std::max(1, (itemCount + PAGE_SIZE - 1) / PAGE_SIZE); }

    uint32_t PriceOf(const ITEM& item)
    {
        uint32_t price = 0;
        Prices::TryGetBuyPrice(item, price);
        return price;
    }

    void LayoutGrid(const std::vector<ITEM*>& items)
    {
        s_scroll = std::clamp(s_scroll, 0, TotalPages(static_cast<int>(items.size())) - 1);
        for (int slot = 0; slot < PAGE_SIZE; ++slot)
        {
            const size_t index = static_cast<size_t>(s_scroll * PAGE_SIZE + slot);
            if (index >= items.size())
                break;

            const int column = slot % GRID_COLUMNS;
            const int row = slot / GRID_COLUMNS;
            s_cells.push_back({ { CENTER_X + column * (GRID_CELL_W + GRID_GAP), CONTENT_Y + row * (GRID_CELL_H + GRID_GAP), GRID_CELL_W, GRID_CELL_H }, items[index] });
        }
    }

    // Groups the pieces of the store by set (the number of the item) and puts the recommended set first.
    void LayoutSets(const std::vector<ITEM*>& items)
    {
        std::vector<int> numbers;
        for (ITEM* item : items)
        {
            const int group = Group(*item);
            if (group >= FIRST_PIECE_GROUP && group < FIRST_PIECE_GROUP + PIECES_PER_SET && std::find(numbers.begin(), numbers.end(), Number(*item)) == numbers.end())
                numbers.push_back(Number(*item));
        }

        const auto isRecommended = [](int number) { return TierOfType(static_cast<uint16_t>(ARMOR_GROUP * CLIENT_GROUP_SIZE + number), 0) != 0; };
        std::stable_sort(numbers.begin(), numbers.end(), [&](int a, int b) { return isRecommended(a) && !isRecommended(b); });
        if (static_cast<int>(numbers.size()) > SET_COLUMNS_MAX)
            numbers.resize(SET_COLUMNS_MAX);

        for (size_t column = 0; column < numbers.size(); ++column)
        {
            SetColumn set;
            const int x = CENTER_X + static_cast<int>(column) * (SET_COL_W + SET_COL_GAP);
            set.armorType = static_cast<uint16_t>(ARMOR_GROUP * CLIENT_GROUP_SIZE + numbers[column]);
            set.recommended = isRecommended(numbers[column]);
            set.rect = { x, CONTENT_Y, SET_COL_W, SET_HEADER_H + PIECES_PER_SET * (SET_CELL_H + 1) + SET_BUY_H + 4 };
            for (int piece = 0; piece < PIECES_PER_SET; ++piece)
            {
                ITEM* found = nullptr;
                for (ITEM* item : items)
                {
                    if (Group(*item) == PIECE_ORDER[piece] && Number(*item) == numbers[column])
                        found = item;
                }

                const Rect cell = { x, CONTENT_Y + SET_HEADER_H + piece * (SET_CELL_H + 1), SET_COL_W, SET_CELL_H };
                set.pieces.push_back({ cell, found });
                if (found != nullptr)
                {
                    set.price += PriceOf(*found);
                    s_cells.push_back({ cell, found });
                }
            }
            set.buyButton = { x, CONTENT_Y + SET_HEADER_H + PIECES_PER_SET * (SET_CELL_H + 1) + 2, SET_COL_W, SET_BUY_H };
            s_columns.push_back(std::move(set));
        }
    }

    void Layout(CNewUIInventoryCtrl* store)
    {
        s_cells.clear();
        s_columns.clear();
        if (s_tab == TAB_FOR_YOU)
            return;

        const std::vector<ITEM*> items = StoreItems(store);
        if (IsSetsTab())
            LayoutSets(items);
        else
            LayoutGrid(items);

        if (s_selected != nullptr && std::none_of(s_cells.begin(), s_cells.end(), [](const Cell& cell) { return cell.item == s_selected; }))
            s_selected = nullptr;
    }

    void FitIconBox(int type, const Rect& box, float& x, float& y, float& w, float& h)
    {
        const ITEM_ATTRIBUTE& attribute = ItemAttribute[type];
        const float naturalW = static_cast<float>(std::max<int>(1, attribute.Width)) * 20.f;
        const float naturalH = static_cast<float>(std::max<int>(1, attribute.Height)) * 20.f;
        const float scale = std::min(static_cast<float>(box.w - 6) / naturalW, static_cast<float>(box.h - 6) / naturalH);
        w = naturalW * scale;
        h = naturalH * scale;
        x = static_cast<float>(box.x) + (static_cast<float>(box.w) - w) * 0.5f;
        y = static_cast<float>(box.y) + (static_cast<float>(box.h) - h) * 0.5f;
    }

    void RenderIcon(int type, int level, BYTE excellent, BYTE ancient, const Rect& box)
    {
        float x, y, w, h;
        FitIconBox(type, box, x, y, w, h);
        glColor4f(1.f, 1.f, 1.f, 1.f);
        RenderItem3D(x, y, w, h, type, level, excellent, ancient, false);
    }

    void TooltipCallback(LPVOID, DWORD, DWORD)
    {
        if (s_hoverItem != nullptr)
            RenderItemInfo(s_hoverX, s_hoverY, s_hoverItem, true);
    }

    void SelectTab(int tab)
    {
        if (tab == s_tab || tab < 0 || tab >= TAB_COUNT)
            return;

        s_tab = tab;
        s_scroll = 0;
        s_selected = nullptr;
        s_previewAskedFor = nullptr;
        s_buyQueue.clear();
        PlayBuffer(SOUND_CLICK01);
        // The server holds the page the vendor shows: the tabs ask it for another one (the "for you" tab keeps the page that is open).
        if (tab != TAB_FOR_YOU)
            SendMobaNpcMenuSelect(static_cast<BYTE>(SERVER_MENU_ID), static_cast<BYTE>(tab - 1));
    }

    // The item to detail on the right: the one under the mouse, else the selected one.
    ITEM* DetailItem() { return s_hoverItem != nullptr ? s_hoverItem : s_selected; }

    void EnqueueSet(const SetColumn& set)
    {
        s_buyQueue.clear();
        for (const Cell& piece : set.pieces)
        {
            if (piece.item != nullptr && !Owns(piece.item->Type))
                s_buyQueue.push_back(piece.item);
        }
    }

    void UpdateHover()
    {
        s_hoverItem = nullptr;
        for (const Cell& cell : s_cells)
        {
            if (!Draw::MouseInRect(cell.rect))
                continue;

            s_hoverItem = cell.item;
            s_hoverX = cell.rect.x + cell.rect.w;
            s_hoverY = cell.rect.y + cell.rect.h / 2;
            return;
        }
    }

    // The copy of an item type the champion carries in the inventory (not worn), or nullptr.
    ITEM* OwnedInInventory(uint16_t type)
    {
        CNewUIInventoryCtrl* inventory = g_pMyInventory != nullptr ? g_pMyInventory->GetInventoryCtrl() : nullptr;
        if (inventory == nullptr)
            return nullptr;

        for (int i = 0; i < inventory->GetNumberOfItems(); ++i)
        {
            ITEM* item = inventory->GetItem(i);
            if (item != nullptr && item->Type == type)
                return item;
        }
        return nullptr;
    }

    // Sells the copy of the shown item the champion carries (the same request as dropping it on the shop).
    void SellOwnedCopy(const ITEM& shown)
    {
        ITEM* owned = OwnedInInventory(shown.Type);
        CNewUIInventoryCtrl* inventory = g_pMyInventory != nullptr ? g_pMyInventory->GetInventoryCtrl() : nullptr;
        if (owned == nullptr || inventory == nullptr || IsHighValueItem(owned))
            return;

        SocketClient->ToGameServer()->SendSellItemToNpcRequest(static_cast<BYTE>(MAX_EQUIPMENT_INDEX + inventory->GetIndexByItem(owned)));
        g_pNPCShop->SetSellingItem(true);
        PlayBuffer(SOUND_CLICK01);
    }

    // Asks the server for the "if you buy it" lines when the selected item changes.
    void RequestPreview(CNewUIInventoryCtrl* store)
    {
        if (s_selected == s_previewAskedFor || store == nullptr)
            return;

        s_previewAskedFor = s_selected;
        Prices::SetPreview(-1, {});
        if (s_selected == nullptr)
            return;

        s_previewSlotAsked = (s_selected->y * store->GetNumberOfColumn()) + s_selected->x;
        s_previewAskedAt = GetTickCount();
        SendMobaShopPreview(static_cast<BYTE>(s_previewSlotAsked));
    }

    // The click in the panel: what it did.
    void HandleClick(CNewUIInventoryCtrl* store, const std::vector<ITEM*>& itemsInStore, UI::Moba::ShopPanel::MouseResult& result)
    {
        if (Draw::MouseInRect(CloseButton()))
        {
            result.close = true;
            return;
        }

        for (int tab = 0; tab < TAB_COUNT; ++tab)
        {
            if (Draw::MouseInRect(TabRect(tab)))
            {
                SelectTab(tab);
                return;
            }
        }

        const std::vector<Prices::Recommended>& steps = Prices::GetRecommended();
        for (size_t i = 0; i < steps.size(); ++i)
        {
            if (Draw::MouseInRect(StepRect(static_cast<int>(i))))
            {
                SelectTab(TabOfPage(steps[i].Page));
                return;
            }
        }

        for (const SetColumn& set : s_columns)
        {
            if (Draw::MouseInRect(set.buyButton))
            {
                EnqueueSet(set);
                PlayBuffer(SOUND_CLICK01);
                return;
            }
        }

        if (IsGridTab())
        {
            const int pages = TotalPages(static_cast<int>(itemsInStore.size()));
            if (Draw::MouseInRect(PrevPageButton()))
                s_scroll = std::max(0, s_scroll - 1);
            else if (Draw::MouseInRect(NextPageButton()))
                s_scroll = std::min(pages - 1, s_scroll + 1);
        }

        if (s_hoverItem != nullptr)
        {
            s_selected = s_hoverItem;
            PlayBuffer(SOUND_CLICK01);
            return;
        }

        if (Draw::MouseInRect(UndoButton()))
        {
            SendMobaShopUndo();
            PlayBuffer(SOUND_CLICK01);
            return;
        }

        if (DetailItem() != nullptr && Draw::MouseInRect(SellButton()))
        {
            SellOwnedCopy(*DetailItem());
            return;
        }

        if (s_selected != nullptr && Draw::MouseInRect(DetailBuyButton()) && store != nullptr && PriceOf(*s_selected) <= static_cast<uint32_t>(CharacterMachine->Gold))
            result.buyItem = s_selected;
    }

    void SetBuyIndex(CNewUIInventoryCtrl* store, UI::Moba::ShopPanel::MouseResult& result)
    {
        if (result.buyItem != nullptr && store != nullptr)
            result.buyIndex = (result.buyItem->y * store->GetNumberOfColumn()) + result.buyItem->x;
    }

    // The next piece of the set being bought, when the previous purchase has been answered.
    void PopQueuedBuy(const std::vector<ITEM*>& itemsInStore, UI::Moba::ShopPanel::MouseResult& result)
    {
        while (!s_buyQueue.empty() && result.buyItem == nullptr)
        {
            ITEM* next = s_buyQueue.front();
            s_buyQueue.erase(s_buyQueue.begin());
            const bool stillThere = std::find(itemsInStore.begin(), itemsInStore.end(), next) != itemsInStore.end();
            if (stillThere && PriceOf(*next) <= static_cast<uint32_t>(CharacterMachine->Gold))
                result.buyItem = next;
        }
    }

    // ---------------------------------------------------------------- drawing

    void RenderFrameAndHeader(const Rect& panel)
    {
        Draw::FillRect(panel, 0.035f, 0.045f, 0.075f, 0.97f);
        Draw::FrameRect(panel, 0.78f, 0.64f, 0.29f, 1.f);
        Draw::FillRect({ panel.x + 1, panel.y + 1, panel.w - 2, HEADER_H - 2 }, 0.08f, 0.10f, 0.16f, 1.f);
    }

    void RenderTabsBack()
    {
        for (int tab = 0; tab < TAB_COUNT; ++tab)
        {
            const Rect rect = TabRect(tab);
            const bool selected = tab == s_tab;
            const bool hover = Draw::MouseInRect(rect);
            Draw::FillRect(rect, selected ? 0.16f : 0.07f, selected ? 0.20f : 0.085f, selected ? 0.31f : 0.12f, 1.f);
            Draw::FrameRect(rect, selected ? 0.95f : 0.50f, selected ? 0.80f : 0.42f, selected ? 0.40f : 0.25f, selected || hover ? 1.f : 0.7f);
        }
    }

    void RenderTabsText(HFONT bold)
    {
        for (int tab = 0; tab < TAB_COUNT; ++tab)
        {
            const Rect rect = TabRect(tab);
            const bool selected = tab == s_tab;
            const BYTE shade = selected ? 255 : 170;
            const bool twoLines = TAB_LABEL_SMALL[tab][0] != L'\0';
            Draw::PutTextCentered(bold, rect.x, rect.y + (twoLines ? 2 : 8), rect.w, TAB_LABEL[tab], shade, tab == TAB_FOR_YOU ? shade : shade, tab == TAB_FOR_YOU ? 120 : shade);
            if (twoLines)
                Draw::PutTextCentered(Draw::Body(), rect.x, rect.y + 14, rect.w, TAB_LABEL_SMALL[tab], 150, 156, 172);
        }
    }

    void RenderStepsBack(unsigned int gold)
    {
        const std::vector<Prices::Recommended>& steps = Prices::GetRecommended();
        const int next = NextStep();
        for (size_t i = 0; i < steps.size(); ++i)
        {
            const Rect row = StepRect(static_cast<int>(i));
            const bool owned = Owns(steps[i].Type);
            const bool isNext = static_cast<int>(i) == next;
            const bool hover = Draw::MouseInRect(row);
            const TierLook& look = TIER_LOOK[std::clamp<int>(steps[i].Tier, 0, 3)];
            Draw::FillRect(row, hover ? 0.12f : 0.07f, hover ? 0.14f : 0.085f, hover ? 0.20f : 0.12f, 1.f);
            Draw::FrameRect(row, isNext ? 0.95f : look.r * 0.8f, isNext ? 0.80f : look.g * 0.8f, isNext ? 0.40f : look.b * 0.8f, 1.f);
            Draw::FillRect({ row.x + 2, row.y + 2, STEP_ICON, STEP_ICON }, 0.04f, 0.05f, 0.08f, 1.f);
            (void)gold;
        }
    }

    void RenderStepsText(HFONT body, HFONT bold, unsigned int gold)
    {
        Draw::PutTextLeft(bold, LEFT_X, CONTENT_Y + 1, LEFT_W, L"BUILD", 255, 214, 80);
        const std::vector<Prices::Recommended>& steps = Prices::GetRecommended();
        const int next = NextStep();
        for (size_t i = 0; i < steps.size(); ++i)
        {
            const Rect row = StepRect(static_cast<int>(i));
            const bool owned = Owns(steps[i].Type);
            const bool isNext = static_cast<int>(i) == next;
            const bool isSet = steps[i].Type / CLIENT_GROUP_SIZE == ARMOR_GROUP;
            const std::wstring name = Shorten(isSet ? L"Set " + SetNameOf(steps[i].Type) : std::wstring(ItemAttribute[steps[i].Type].Name), STEP_NAME_CHARS);
            Draw::PutTextLeft(bold, row.x + STEP_ICON + 6, row.y + 2, row.w - STEP_ICON - 8, name.c_str(), owned ? 150 : 235, owned ? 190 : 235, owned ? 150 : 235);

            wchar_t line[48] = {};
            if (owned)
                swprintf(line, std::size(line), L"Comprado");
            else
                swprintf(line, std::size(line), L"%ls  %u", isNext ? L"Siguiente" : L"Luego", steps[i].Price);
            const bool affordable = gold >= steps[i].Price;
            const BYTE r = owned ? 100 : (isNext ? 255 : 170);
            const BYTE g = owned ? 220 : (isNext ? 214 : 175);
            const BYTE b = owned ? 120 : (isNext ? 80 : 190);
            Draw::PutTextLeft(body, row.x + STEP_ICON + 6, row.y + 14, row.w - STEP_ICON - 8, line, r, g, b);
            if (!owned && !affordable)
                Draw::PutTextLeft(body, row.x + STEP_ICON + 6, row.y + 25, row.w - STEP_ICON - 8, L"te falta oro", 255, 110, 110);
        }
    }

    void RenderStepIcons()
    {
        const std::vector<Prices::Recommended>& steps = Prices::GetRecommended();
        for (size_t i = 0; i < steps.size(); ++i)
        {
            const Rect row = StepRect(static_cast<int>(i));
            RenderIcon(steps[i].Type, 0, 0, 0, { row.x + 2, row.y + 2, STEP_ICON, STEP_ICON });
        }
    }

    // The tab "for you": the next step in big, and the rules in a few lines.
    void RenderForYou(HFONT body, HFONT bold, unsigned int gold)
    {
        const int next = NextStep();
        const std::vector<Prices::Recommended>& steps = Prices::GetRecommended();
        const Rect card = { CENTER_X, CONTENT_Y, CENTER_W, 96 };
        Draw::FillRect(card, 0.08f, 0.09f, 0.13f, 1.f);
        Draw::FrameRect(card, 0.95f, 0.80f, 0.40f, 1.f);
        EndRenderColor();
        Draw::PutTextLeft(bold, card.x + 8, card.y + 6, card.w - 16, next < 0 ? L"\u00a1Ya tienes todo lo recomendado!" : L"TU SIGUIENTE COMPRA", 255, 214, 80);
        if (next >= 0)
        {
            const Prices::Recommended& step = steps[static_cast<size_t>(next)];
            const bool isSet = step.Type / CLIENT_GROUP_SIZE == ARMOR_GROUP;
            const std::wstring name = isSet ? L"Set " + SetNameOf(step.Type) : std::wstring(ItemAttribute[step.Type].Name);
            Draw::PutTextLeft(bold, card.x + 80, card.y + 28, card.w - 88, name.c_str(), 240, 240, 240);
            wchar_t line[64] = {};
            swprintf(line, std::size(line), L"Tier %d  -  %u de oro", static_cast<int>(step.Tier), step.Price);
            Draw::PutTextLeft(body, card.x + 80, card.y + 46, card.w - 88, line, gold >= step.Price ? 140 : 255, gold >= step.Price ? 235 : 110, gold >= step.Price ? 140 : 110);
            Draw::PutTextLeft(body, card.x + 80, card.y + 62, card.w - 88, L"Clic en el paso de la izquierda", 170, 175, 190);
            Draw::PutTextLeft(body, card.x + 80, card.y + 76, card.w - 88, L"para ir a la p\u00e1gina donde se vende.", 170, 175, 190);
        }

        static const wchar_t* const TIPS[] = {
            L"Compra de arriba hacia abajo: lo que dice 'Comprado'",
            L"ya lo tienes (puesto o en tu inventario).",
            L"Un set son cinco piezas; 'Comprar set' compra las que te falten.",
            L"Los tiers suben el poder: T1 al empezar, T2 a media partida,",
            L"T3 al final. Cada set tiene un rol (tanque, ofensivo...).",
            L"Pasa el mouse sobre un \u00edtem para ver todas sus opciones.",
        };
        for (size_t i = 0; i < std::size(TIPS); ++i)
            Draw::PutTextLeft(body, CENTER_X + 4, CONTENT_Y + 108 + static_cast<int>(i) * INFO_LINE_H, CENTER_W - 8, TIPS[i], 200, 205, 215);
    }

    void RenderGridBack(unsigned int gold)
    {
        for (const Cell& cell : s_cells)
        {
            const int tier = Prices::TierOf(*cell.item);
            const TierLook& look = TIER_LOOK[tier];
            const bool hover = cell.item == s_hoverItem;
            const bool selected = cell.item == s_selected;
            Draw::FillRect(cell.rect, hover ? 0.16f : 0.08f, hover ? 0.18f : 0.095f, hover ? 0.26f : 0.14f, 1.f);
            Draw::FrameRect(cell.rect, selected ? 0.95f : look.r, selected ? 0.80f : look.g, selected ? 0.40f : look.b, hover || selected ? 1.f : 0.7f);
        }
        (void)gold;
    }

    void RenderCellText(HFONT body, HFONT bold, const Cell& cell, unsigned int gold, bool compact)
    {
        wchar_t text[32] = {};
        const int tier = Prices::TierOf(*cell.item);
        const TierLook& look = TIER_LOOK[tier];
        if (compact)
        {
            uint32_t cost = 0;
            if (Prices::TryGetBuyPrice(*cell.item, cost))
            {
                swprintf(text, std::size(text), L"%u", cost);
                const bool ok = gold >= cost;
                Draw::PutText(bold, cell.rect.x, cell.rect.y + cell.rect.h - 13, cell.rect.w - 2, text, ok ? 120 : 255, ok ? 235 : 110, ok ? 130 : 110, RT3_SORT_RIGHT);
            }
            return;
        }

        if (tier > 0)
            Draw::PutTextLeft(body, cell.rect.x + 3, cell.rect.y + 1, 22, look.label, look.tr, look.tg, look.tb);
        if (cell.item->Level > 0 && tier > 0)
        {
            swprintf(text, std::size(text), L"+%d", cell.item->Level);
            Draw::PutText(body, cell.rect.x + cell.rect.w - 26, cell.rect.y + 1, 24, text, 235, 235, 235, RT3_SORT_RIGHT);
        }

        uint32_t price = 0;
        if (Prices::TryGetBuyPrice(*cell.item, price))
        {
            swprintf(text, std::size(text), L"%u", price);
            const bool affordable = gold >= price;
            Draw::PutTextCentered(bold, cell.rect.x, cell.rect.y + cell.rect.h - 15, cell.rect.w, text, affordable ? 120 : 255, affordable ? 235 : 110, affordable ? 130 : 110);
        }

        if (tier == 0 && cell.item->Durability > 1)
        {
            swprintf(text, std::size(text), L"x%d", cell.item->Durability);
            Draw::PutText(body, cell.rect.x + cell.rect.w - 26, cell.rect.y + cell.rect.h - 28, 24, text, 235, 235, 235, RT3_SORT_RIGHT);
        }
    }

    void RenderSetsBack()
    {
        for (const SetColumn& set : s_columns)
        {
            Draw::FillRect(set.rect, 0.05f, 0.06f, 0.09f, 1.f);
            Draw::FrameRect(set.rect, set.recommended ? 0.95f : 0.35f, set.recommended ? 0.80f : 0.30f, set.recommended ? 0.40f : 0.22f, 1.f);
            for (const Cell& piece : set.pieces)
            {
                const bool hover = piece.item != nullptr && piece.item == s_hoverItem;
                const bool selected = piece.item != nullptr && piece.item == s_selected;
                Draw::FillRect(piece.rect, hover ? 0.16f : 0.08f, hover ? 0.18f : 0.095f, hover ? 0.26f : 0.14f, piece.item != nullptr ? 1.f : 0.4f);
                if (selected)
                    Draw::FrameRect(piece.rect, 0.95f, 0.80f, 0.40f, 1.f);
            }
            Draw::FillRect(set.buyButton, 0.18f, 0.36f, 0.22f, 1.f);
            Draw::FrameRect(set.buyButton, 0.40f, 0.78f, 0.45f, 1.f);
        }
    }

    void RenderSetsText(HFONT body, HFONT bold, unsigned int gold)
    {
        for (const SetColumn& set : s_columns)
        {
            const std::wstring name = SetNameOf(set.armorType);
            Draw::PutTextCentered(bold, set.rect.x, set.rect.y + 2, set.rect.w, name.c_str(), 240, 235, 215);
            const int role = Prices::SetRoleOf(set.armorType);
            if (role >= 0 && role < ROLE_COUNT)
                Draw::PutTextCentered(body, set.rect.x, set.rect.y + 15, set.rect.w, ROLE_LABEL[role], 160, 190, 235);
            if (set.recommended)
                Draw::PutTextCentered(bold, set.rect.x, set.rect.y + SET_HEADER_H - 12, set.rect.w, L"REC.", 255, 214, 80);
            for (const Cell& piece : set.pieces)
            {
                if (piece.item != nullptr)
                    RenderCellText(body, bold, piece, gold, true);
            }

            wchar_t text[32] = {};
            swprintf(text, std::size(text), L"SET %u", set.price);
            const bool affordable = gold >= set.price;
            Draw::PutTextCentered(body, set.buyButton.x, set.buyButton.y + 4, set.buyButton.w, text, affordable ? 235 : 255, affordable ? 235 : 150, affordable ? 235 : 150);
        }
    }

    // The strip below the catalog: the item under the mouse (or the selected one), what changes if it is bought and the buttons.
    void RenderDetailBack(const ITEM* item, unsigned int gold)
    {
        const Rect strip = Strip();
        Draw::FillRect(strip, 0.05f, 0.06f, 0.09f, 1.f);
        Draw::FrameRect(strip, 0.35f, 0.30f, 0.22f, 1.f);
        for (const Rect& button : { SellButton(), UndoButton() })
        {
            const bool hover = Draw::MouseInRect(button);
            Draw::FillRect(button, hover ? 0.16f : 0.10f, hover ? 0.18f : 0.115f, hover ? 0.26f : 0.15f, 1.f);
            Draw::FrameRect(button, 0.55f, 0.46f, 0.25f, 1.f);
        }
        if (item == nullptr)
            return;

        Draw::FillRect(StripIcon(), 0.08f, 0.095f, 0.14f, 1.f);
        const Rect buy = DetailBuyButton();
        const bool canBuy = PriceOf(*item) <= gold;
        Draw::FillRect(buy, canBuy ? 0.18f : 0.16f, canBuy ? 0.36f : 0.14f, canBuy ? 0.22f : 0.14f, 1.f);
        Draw::FrameRect(buy, 0.40f, canBuy ? 0.78f : 0.30f, canBuy ? 0.45f : 0.30f, 1.f);
    }

    const wchar_t* PreviewLabel(int stat)
    {
        switch (stat)
        {
        case 1: return L"Vida m\u00e1x.";
        case 2: return L"Man\u00e1 m\u00e1x.";
        case 3: return L"Defensa";
        case 4: return L"Da\u00f1o f\u00edsico";
        case 5: return L"Da\u00f1o m\u00e1gico";
        case 6: return L"Escudo m\u00e1x.";
        default: return L"Vel. ataque";
        }
    }

    // "If you buy it": the stats of the champion before and after wearing the item (the numbers come from the server).
    void RenderPreview(HFONT body, HFONT bold, const ITEM* item)
    {
        const int x = PANEL_X + PAD + STRIP_PREVIEW_X;
        const int y = STRIP_Y + 4;
        Draw::PutTextLeft(bold, x, y, 140, L"SI LO COMPRAS", 255, 214, 80);
        if (item != s_selected)
        {
            Draw::PutTextLeft(body, x, y + 16, 140, L"Haz clic para ver el cambio.", 170, 175, 190);
            return;
        }

        const bool answered = s_previewAskedFor == s_selected && Prices::PreviewSlot() == s_previewSlotAsked;
        if (!answered)
        {
            const bool late = GetTickCount() - s_previewAskedAt > PREVIEW_TIMEOUT_MS;
            Draw::PutTextLeft(body, x, y + 16, 140, late ? L"Sin respuesta del servidor." : L"Calculando...", 170, 175, 190);
            return;
        }

        const std::vector<Prices::PreviewLine>& lines = Prices::PreviewLines();
        if (lines.empty())
        {
            Draw::PutTextLeft(body, x, y + 16, 140, L"Sin cambios en tus stats.", 170, 175, 190);
            return;
        }

        int row = 0;
        for (const Prices::PreviewLine& line : lines)
        {
            if (row >= PREVIEW_MAX_LINES)
                break;

            const int lineY = y + 16 + row * INFO_LINE_H;
            Draw::PutTextLeft(body, x, lineY, 58, PreviewLabel(line.Stat), 190, 195, 205);
            wchar_t text[48] = {};
            swprintf(text, std::size(text), L"%u", line.Before);
            Draw::PutText(body, x + 58, lineY, 32, text, 235, 235, 235, RT3_SORT_RIGHT);
            Draw::PutTextCentered(body, x + 92, lineY, 12, L">", 150, 155, 170);
            swprintf(text, std::size(text), L"%u", line.After);
            const bool up = line.After >= line.Before;
            Draw::PutText(body, x + 104, lineY, 36, text, up ? 120 : 255, up ? 235 : 110, up ? 130 : 110, RT3_SORT_RIGHT);
            ++row;
        }
    }

    void RenderDetailText(HFONT body, HFONT bold, const ITEM* item, unsigned int gold)
    {
        const int infoX = PANEL_X + PAD + STRIP_INFO_X;
        const int infoW = STRIP_PREVIEW_X - STRIP_INFO_X - 4;
        const Rect sell = SellButton();
        const Rect undo = UndoButton();
        const bool ownsCopy = item != nullptr && OwnedInInventory(item->Type) != nullptr;
        Draw::PutTextCentered(bold, sell.x, sell.y + 4, sell.w, L"Vender", ownsCopy ? 240 : 120, ownsCopy ? 240 : 120, ownsCopy ? 240 : 125);
        Draw::PutTextCentered(bold, undo.x, undo.y + 4, undo.w, L"Deshacer", 240, 240, 240);
        if (item == nullptr)
        {
            Draw::PutTextLeft(bold, PANEL_X + PAD + 8, STRIP_Y + 4, 200, L"DETALLE", 255, 214, 80);
            Draw::PutTextLeft(body, PANEL_X + PAD + 8, STRIP_Y + 22, 300, L"Haz clic en un \u00edtem para ver su precio, lo que cambia", 190, 195, 205);
            Draw::PutTextLeft(body, PANEL_X + PAD + 8, STRIP_Y + 36, 300, L"en tus stats y comprarlo. Vender: clic derecho en tu", 190, 195, 205);
            Draw::PutTextLeft(body, PANEL_X + PAD + 8, STRIP_Y + 50, 300, L"inventario.", 190, 195, 205);
            return;
        }

        const int tier = Prices::TierOf(*item);
        const TierLook& look = TIER_LOOK[tier];
        const uint32_t price = PriceOf(*item);
        const bool canBuy = price <= gold;
        wchar_t line[64] = {};
        Draw::PutTextLeft(bold, infoX, STRIP_Y + 3, infoW, Shorten(ItemAttribute[item->Type].Name, 17).c_str(), 240, 240, 240);
        if (tier > 0)
            swprintf(line, std::size(line), L"Tier %d%ls", tier, item->Level > 0 ? L"  +" : L"");
        if (tier > 0 && item->Level > 0)
            swprintf(line, std::size(line), L"Tier %d  +%d", tier, static_cast<int>(item->Level));
        if (tier > 0)
            Draw::PutTextLeft(bold, infoX, STRIP_Y + 17, infoW, line, look.tr, look.tg, look.tb);
        const int role = Group(*item) >= FIRST_PIECE_GROUP && Group(*item) < FIRST_PIECE_GROUP + PIECES_PER_SET
            ? Prices::SetRoleOf(static_cast<uint16_t>(ARMOR_GROUP * CLIENT_GROUP_SIZE + Number(*item))) : -1;
        if (role >= 0 && role < ROLE_COUNT)
            Draw::PutTextLeft(body, infoX, STRIP_Y + 31, infoW, ROLE_LABEL[role], 160, 190, 235);
        swprintf(line, std::size(line), L"Precio: %u", price);
        Draw::PutTextLeft(bold, infoX, STRIP_Y + 45, infoW, line, 255, 214, 80);
        if (canBuy)
            swprintf(line, std::size(line), L"Te quedar\u00edan %u", gold - price);
        else
            swprintf(line, std::size(line), L"Te faltan %u", price - gold);
        Draw::PutTextLeft(body, infoX, STRIP_Y + 59, infoW, line, canBuy ? 140 : 255, canBuy ? 235 : 110, canBuy ? 140 : 110);
        if (Owns(item->Type) && role < 0)
            Draw::PutTextLeft(body, infoX, STRIP_Y + 31, infoW, L"Ya lo tienes", 140, 230, 150);

        RenderPreview(body, bold, item);
        const Rect buy = DetailBuyButton();
        Draw::PutTextCentered(bold, buy.x, buy.y + 5, buy.w, L"COMPRAR", 240, 240, 240);
    }

    void RenderPager(HFONT bold, int itemCount)
    {
        if (!IsGridTab())
            return;

        const int pages = TotalPages(itemCount);
        const Rect prev = PrevPageButton();
        const Rect next = NextPageButton();
        Draw::Button(prev, L"<", s_scroll > 0);
        Draw::Button(next, L"> ", s_scroll < pages - 1);
        wchar_t text[16] = {};
        swprintf(text, std::size(text), L"%d/%d", s_scroll + 1, pages);
        Draw::PutTextCentered(bold, prev.x + prev.w, prev.y + 2, next.x - prev.x - prev.w, text, 235, 235, 235);
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
    s_scroll = 0;
    s_hoverItem = nullptr;
    s_selected = nullptr;
    s_previewAskedFor = nullptr;
    s_buyQueue.clear();
}

void SetPageTitle(const std::wstring& title)
{
    s_pageTitle = title;
}

void ClearPageTitle()
{
    s_pageTitle.clear();
    s_tab = TAB_FOR_YOU;
}

bool Contains(int x, int y)
{
    return x >= PANEL_X && x < PANEL_X + PANEL_W && y >= PANEL_Y && y < PANEL_Y + PANEL_H;
}

MouseResult UpdateMouse(CNewUIInventoryCtrl* store, bool clicksEnabled, bool canBuy)
{
    MouseResult result;
    s_hoverItem = nullptr;
    if (!Active() || store == nullptr)
        return result;

    result.used = Contains(MouseX, MouseY);
    if (!result.used)
        return result;

    std::vector<ITEM*> items = StoreItems(store);
    if (MouseWheel != 0 && IsGridTab())
        s_scroll = std::clamp(s_scroll + (MouseWheel > 0 ? -1 : 1), 0, TotalPages(static_cast<int>(items.size())) - 1);

    Layout(store);
    UpdateHover();
    if (clicksEnabled && SEASON3B::IsRelease(VK_LBUTTON))
    {
        HandleClick(store, items, result);
        Layout(store);
    }
    RequestPreview(store);

    if (canBuy)
        PopQueuedBuy(items, result);
    SetBuyIndex(store, result);
    return result;
}

void Render(CNewUIInventoryCtrl* store)
{
    if (!Active() || store == nullptr)
        return;

    Layout(store);
    const unsigned int gold = static_cast<unsigned int>(CharacterMachine->Gold);
    const Rect panel = Panel();

    RenderFrameAndHeader(panel);
    RenderTabsBack();
    RenderStepsBack(gold);
    if (IsSetsTab())
        RenderSetsBack();
    else if (IsGridTab())
        RenderGridBack(gold);
    RenderDetailBack(DetailItem(), gold);
    EndRenderColor();

    // Texts (drawn after the colors so the font state is not mixed with the color batch).
    const HFONT body = Draw::Body();
    const HFONT bold = Draw::Bold();
    Draw::PutTextLeft(Draw::Title(), panel.x + PAD, panel.y + 4, 200, L"TIENDA", 255, 226, 150);
    wchar_t text[64] = {};
    swprintf(text, std::size(text), L"Oro: %u", gold);
    Draw::PutText(bold, panel.x + 150, panel.y + 7, panel.w - 150 - CLOSE_SIZE - 14, text, 255, 214, 80, RT3_SORT_RIGHT);
    Draw::PutTextCentered(bold, panel.x + panel.w - CLOSE_SIZE - 6, panel.y + 7, CLOSE_SIZE, L"X", 235, 235, 235);
    RenderTabsText(bold);
    RenderStepsText(body, bold, gold);

    if (s_tab == TAB_FOR_YOU)
    {
        RenderForYou(body, bold, gold);
    }
    else if (IsSetsTab())
    {
        RenderSetsText(body, bold, gold);
    }
    else
    {
        for (const Cell& cell : s_cells)
            RenderCellText(body, bold, cell, gold, false);
        RenderPager(bold, static_cast<int>(StoreItems(store).size()));
    }

    RenderDetailText(body, bold, DetailItem(), gold);
    Draw::PutTextLeft(body, panel.x + PAD, panel.y + panel.h - 16, panel.w - 2 * PAD,
        L"Clic: ver detalle.  La joya de perfecci\u00f3n se arrastra sobre un \u00edtem de tu inventario.", 170, 175, 190);

    if (const std::vector<Prices::Recommended>& steps = Prices::GetRecommended(); !steps.empty())
    {
        for (size_t i = 0; i < steps.size(); ++i)
        {
            if (Draw::MouseInRect(StepRect(static_cast<int>(i))))
                Draw::Tooltip(L"Clic: ir a la p\u00e1gina donde se vende");
        }
    }
}

void Render3D(CNewUIInventoryCtrl* store)
{
    if (!Active() || store == nullptr)
        return;

    Layout(store);
    RenderStepIcons();
    for (const Cell& cell : s_cells)
    {
        const Rect box = IsSetsTab() ? Rect{ cell.rect.x + 14, cell.rect.y + 1, cell.rect.w - 28, cell.rect.h - 2 } : Rect{ cell.rect.x, cell.rect.y + 4, cell.rect.w, cell.rect.h - 18 };
        RenderIcon(cell.item->Type, cell.item->Level, cell.item->ExcellentFlags, cell.item->AncientDiscriminator, box);
    }

    if (const ITEM* item = DetailItem(); item != nullptr)
        RenderIcon(item->Type, item->Level, item->ExcellentFlags, item->AncientDiscriminator, StripIcon());

    if (s_tab == TAB_FOR_YOU)
    {
        const int next = NextStep();
        if (next >= 0)
            RenderIcon(Prices::GetRecommended()[static_cast<size_t>(next)].Type, 0, 0, 0, { CENTER_X + 8, CONTENT_Y + 22, 64, 64 });
    }
}

void QueueTooltip()
{
    if (s_hoverItem != nullptr && g_pNewUI3DRenderMng != nullptr)
        g_pNewUI3DRenderMng->RenderUI2DEffect(INVENTORY_CAMERA_Z_ORDER, TooltipCallback, nullptr, 0, 0);
}

}
