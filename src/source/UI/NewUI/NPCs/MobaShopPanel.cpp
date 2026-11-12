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
#include "App/Platform/Windows/Winmain.h"     // g_hFont, g_hFontBold (the tooltip of an item)
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
    constexpr int GRID_CELL_W = 49;
    constexpr int GRID_CELL_H = 44;
    // A quantity ("x100") is only shown for what stacks: the durability of pets, wings and the like (200, 255) means nothing to the player.
    constexpr int MAX_SHOWN_QUANTITY = 100;
    constexpr int GRID_GAP = 3;
    constexpr int PAGE_SIZE = GRID_COLUMNS * GRID_ROWS;

    // The catalog pages are cut into sections with a title (the tiers of the weapons, the accessories and the wings of the consumables...).
    // Small squares for the cheap things, big ones (icon on the left, name, level and price on the right) for weapons, wings and shields.
    constexpr int BIG_COLUMNS = 3;
    constexpr int BIG_CELL_H = 58;
    constexpr int BIG_ICON_W = 48;
    constexpr int BIG_NAME_CHARS = 11;
    constexpr int SECTION_H = 15;
    constexpr int SECTION_GAP = 4;
    constexpr int SCROLLBAR_W = 5;
    constexpr int PEEK_MIN_H = 8;          // a row that starts at least this far above the bottom is drawn, cut at the edge, so the player sees there is more
    constexpr int MIN_THUMB_H = 16;
    constexpr int WINGS_GROUP = 12;
    constexpr int CAPE_OF_LORD_GROUP = 13;
    constexpr int CAPE_OF_LORD_NUMBER = 30;

    // The sets: one column per set, its five pieces from top to bottom.
    constexpr int SET_COLUMNS_MAX = 5;
    constexpr int SET_COL_W = 63;
    constexpr int SET_COL_GAP = 2;
    constexpr int SET_HEADER_H = 42;
    constexpr int SET_CELL_H = 34;
    constexpr int SET_NAME_CHARS = 10;
    // The armor piece sits higher in its square than the other pieces (the 3D model has its own anchor): lowered by this much.
    constexpr int SET_ARMOR_SHIFT = 6;
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
    constexpr int STRIP_PREVIEW_X = 178;
    constexpr int STRIP_NAME_CHARS = 16;
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
        L"", L"y acces.", L"", L"", L"", L"", L"escudos, alas",
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
        bool big = false;
    };

    struct SectionHeader
    {
        Rect rect;
        const wchar_t* title;
        int tier;
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
    std::vector<SectionHeader> s_headers;    // the titles of the sections of the page being shown
    int s_maxScroll = 0;                     // the last first-row the catalog can scroll to (0 = everything fits)
    int s_rowTotal = 0;                      // rows of the catalog (titles and rows of squares)
    int s_rowShown = 0;                      // rows drawn at the current scroll
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

    uint32_t PriceOf(const ITEM& item)
    {
        uint32_t price = 0;
        Prices::TryGetBuyPrice(item, price);
        return price;
    }

    // ---------------------------------------------------------------- sections of the catalog pages

    // Wings and capes: the Dark Lord's Cape of Lord is in the group of the pets (13), number 30.
    bool IsWing(const ITEM& item) { return Group(item) == WINGS_GROUP || (Group(item) == CAPE_OF_LORD_GROUP && Number(item) == CAPE_OF_LORD_NUMBER); }

    struct SectionDef
    {
        const wchar_t* title;
        bool big;
        int tier;                                      // the colour of the title (0 = neutral)
        bool (*accepts)(const ITEM& item, int tier);   // the first section that accepts an item takes it
    };

    const SectionDef WEAPON_SECTIONS[] = {
        { L"TIER 1  -  para empezar", true, 1, [](const ITEM&, int tier) { return tier == 1; } },
        { L"TIER 2  -  media partida", true, 2, [](const ITEM&, int tier) { return tier == 2; } },
        { L"TIER 3  -  final de partida", true, 3, [](const ITEM&, int tier) { return tier == 3; } },
    };

    const SectionDef CONSUMABLE_SECTIONS[] = {
        { L"CONSUMIBLES", false, 0, [](const ITEM& item, int tier) { return tier == 0 && !IsWing(item); } },
        { L"ACCESORIOS", false, 0, [](const ITEM& item, int tier) { return tier > 0 && !IsWing(item); } },
    };

    // Pets, shields and wings share one page (2026-11-12).
    const SectionDef PET_SECTIONS[] = {
        { L"MASCOTAS", false, 0, [](const ITEM& item, int tier) { return tier == 0 && !IsWing(item); } },
        { L"ESCUDOS", true, 0, [](const ITEM& item, int tier) { return tier > 0 && !IsWing(item); } },
        { L"ALAS", true, 0, [](const ITEM& item, int) { return IsWing(item); } },
    };

    const SectionDef OTHER_SECTION = { L"OTROS", false, 0, [](const ITEM&, int) { return true; } };

    std::vector<SectionDef> SectionsOfTab()
    {
        std::vector<SectionDef> sections;
        if (s_tab == TAB_WEAPONS)
            sections.assign(std::begin(WEAPON_SECTIONS), std::end(WEAPON_SECTIONS));
        else if (s_tab == TAB_CONSUMABLES)
            sections.assign(std::begin(CONSUMABLE_SECTIONS), std::end(CONSUMABLE_SECTIONS));
        else
            sections.assign(std::begin(PET_SECTIONS), std::end(PET_SECTIONS));
        sections.push_back(OTHER_SECTION);
        return sections;
    }

    // The catalog is a column of rows (a title, or a row of squares) that scrolls with the mouse wheel.
    struct CatalogRow
    {
        int section;        // index in the sections of the tab
        size_t first;       // first item of the row (squares rows)
        size_t count;       // items in the row; 0 for a title
        int height;         // including the gap below
    };

    int CatalogCellW(bool big)
    {
        const int width = CENTER_W - SCROLLBAR_W - 2;
        return big ? (width - (BIG_COLUMNS - 1) * GRID_GAP) / BIG_COLUMNS : (width - (GRID_COLUMNS - 1) * GRID_GAP) / GRID_COLUMNS;
    }

    std::vector<CatalogRow> RowsOf(const std::vector<SectionDef>& sections, const std::vector<std::vector<ITEM*>>& buckets)
    {
        std::vector<CatalogRow> rows;
        for (size_t section = 0; section < sections.size(); ++section)
        {
            const size_t itemCount = buckets[section].size();
            if (itemCount == 0)
                continue;

            const bool big = sections[section].big;
            const size_t columns = big ? BIG_COLUMNS : GRID_COLUMNS;
            rows.push_back({ static_cast<int>(section), 0, 0, SECTION_H });
            for (size_t first = 0; first < itemCount; first += columns)
                rows.push_back({ static_cast<int>(section), first, std::min(columns, itemCount - first), (big ? BIG_CELL_H : GRID_CELL_H) + GRID_GAP });
            rows.back().height += SECTION_GAP;
        }
        return rows;
    }

    // The first row that can be at the top while the rows below it still fill the window down to the last one.
    int LastScrollStart(const std::vector<CatalogRow>& rows, int room)
    {
        int used = 0;
        int start = static_cast<int>(rows.size());
        while (start > 0 && used + rows[static_cast<size_t>(start) - 1].height - GRID_GAP <= room)
        {
            --start;
            used += rows[static_cast<size_t>(start)].height;
        }
        return std::min(start, std::max(0, static_cast<int>(rows.size()) - 1));
    }

    void LayoutGrid(const std::vector<ITEM*>& items)
    {
        const std::vector<SectionDef> sections = SectionsOfTab();
        std::vector<std::vector<ITEM*>> buckets(sections.size());
        for (ITEM* item : items)
        {
            const int tier = Prices::TierOf(*item);
            for (size_t i = 0; i < sections.size(); ++i)
            {
                if (sections[i].accepts(*item, tier))
                {
                    buckets[i].push_back(item);
                    break;
                }
            }
        }

        const std::vector<CatalogRow> rows = RowsOf(sections, buckets);
        const int bottom = STRIP_Y - 4;
        s_maxScroll = LastScrollStart(rows, bottom - CONTENT_Y);
        s_scroll = std::clamp(s_scroll, 0, s_maxScroll);

        int y = CONTENT_Y;
        s_rowShown = 0;
        for (size_t r = static_cast<size_t>(s_scroll); r < rows.size(); ++r)
        {
            const CatalogRow& row = rows[r];
            // The next row is drawn even if it does not fit: it is cut at the bottom edge (see CatalogClip) and shows that the page scrolls.
            if (y > bottom - PEEK_MIN_H)
                break;
            ++s_rowShown;

            const SectionDef& def = sections[static_cast<size_t>(row.section)];
            if (row.count == 0)
            {
                s_headers.push_back({ { CENTER_X, y, CENTER_W - SCROLLBAR_W - 2, SECTION_H }, def.title, def.tier });
            }
            else
            {
                const int cellW = CatalogCellW(def.big);
                const int cellH = def.big ? BIG_CELL_H : GRID_CELL_H;
                for (size_t k = 0; k < row.count; ++k)
                    s_cells.push_back({ { CENTER_X + static_cast<int>(k) * (cellW + GRID_GAP), y, cellW, cellH }, buckets[static_cast<size_t>(row.section)][row.first + k], def.big });
            }
            y += row.height;
        }
        s_rowTotal = static_cast<int>(rows.size());
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
        s_headers.clear();
        s_maxScroll = 0;
        s_rowTotal = 0;
        s_rowShown = 0;
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

    // Clips what is drawn while it lives to a box of the panel (reference coordinates).
    // The 3D models of the items have their own fixed scale (long staffs, wide wings) and ignore the size asked for, so they spilled over the tabs and the neighbours.
    class IconClip
    {
    public:
        explicit IconClip(const Rect& box)
        {
            const int left = static_cast<int>(box.x * g_fScreenRate_x);
            const int bottom = static_cast<int>(WindowHeight - (box.y + box.h) * g_fScreenRate_y);
            const int width = static_cast<int>(box.w * g_fScreenRate_x);
            const int height = static_cast<int>(box.h * g_fScreenRate_y);
            EnableScissor(left, bottom, width, height);
        }

        ~IconClip() { DisableScissor(); }

        IconClip(const IconClip&) = delete;
        IconClip& operator=(const IconClip&) = delete;
    };

    // Where the icon of a square goes: inside the set pieces, left of the big squares, above the price of the small ones.
    Rect IconBoxOf(const Cell& cell)
    {
        if (IsSetsTab())
            return { cell.rect.x + 14, cell.rect.y + 1, cell.rect.w - 28, cell.rect.h - 2 };
        if (cell.big)
            return { cell.rect.x + 2, cell.rect.y + 3, BIG_ICON_W, cell.rect.h - 6 };
        return { cell.rect.x, cell.rect.y + 4, cell.rect.w, cell.rect.h - 18 };
    }

    // The armor of a set is drawn higher than the other pieces by the 3D anchor of its model: this lowers it.
    int IconShiftOf(const Cell& cell)
    {
        return IsSetsTab() && Group(*cell.item) == ARMOR_GROUP ? SET_ARMOR_SHIFT : 0;
    }

    // The area of the catalog: what is drawn below it (the next row, peeking) is cut.
    Rect CatalogArea() { return { CENTER_X - 1, CONTENT_Y, CENTER_W + 2, STRIP_Y - 4 - CONTENT_Y }; }

    void RenderIcon(int type, int level, BYTE excellent, BYTE ancient, const Rect& box, int shiftDown = 0, int clipBottom = REFERENCE_HEIGHT)
    {
        float x, y, w, h;
        FitIconBox(type, box, x, y, w, h);
        y += static_cast<float>(shiftDown);
        glColor4f(1.f, 1.f, 1.f, 1.f);
        Rect clipBox = box;
        clipBox.h = std::min(clipBox.h, clipBottom - clipBox.y);
        if (clipBox.h <= 0)
            return;
        const IconClip clip(clipBox);
        RenderItem3D(x, y, w, h, type, level, excellent, ancient, false);
    }

    void TooltipCallback(LPVOID, DWORD, DWORD)
    {
        if (s_hoverItem == nullptr)
            return;

        // The legacy tooltip draws with the two global fonts, which are small: it gets the larger ones for this call.
        const HFONT normalFont = g_hFont;
        const HFONT boldFont = g_hFontBold;
        g_hFont = Draw::TooltipBody();
        g_hFontBold = Draw::TooltipBold();
        RenderItemInfo(s_hoverX, s_hoverY, s_hoverItem, true);
        g_hFont = normalFont;
        g_hFontBold = boldFont;
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
            if (!Draw::MouseInRect(cell.rect) || (!IsSetsTab() && MouseY >= CatalogArea().y + CatalogArea().h))
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
        // No high-value confirmation here: everything this shop sells is high value (+13...+15, excellent) and the silent refusal made selling impossible.
        if (owned == nullptr || inventory == nullptr)
            return;

        SocketClient->ToGameServer()->SendSellItemToNpcRequest(static_cast<BYTE>(inventory->GetIndexByItem(owned))); // the index already counts the equipment slots
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

    // A right click on an item buys it at once (without the button of the detail strip) when the champion can pay it.
    void HandleRightClick(UI::Moba::ShopPanel::MouseResult& result)
    {
        if (s_hoverItem == nullptr)
            return;

        s_selected = s_hoverItem;
        if (PriceOf(*s_hoverItem) <= static_cast<uint32_t>(CharacterMachine->Gold))
            result.buyItem = s_hoverItem;
        PlayBuffer(SOUND_CLICK01);
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

    void RenderScrollbar();

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

        for (const SectionHeader& header : s_headers)
        {
            const TierLook& look = TIER_LOOK[std::clamp(header.tier, 0, 3)];
            Draw::FillRect({ header.rect.x, header.rect.y + header.rect.h - 2, header.rect.w, 1 }, look.r * 0.7f, look.g * 0.7f, look.b * 0.7f, 1.f);
        }
        RenderScrollbar();
        (void)gold;
    }

    // The titles of the sections ("TIER 1 - para empezar", "ALAS"...), in the colour of their tier.
    void RenderSectionTitles(HFONT bold)
    {
        for (const SectionHeader& header : s_headers)
        {
            const TierLook& look = TIER_LOOK[std::clamp(header.tier, 0, 3)];
            Draw::PutTextLeft(bold, header.rect.x + 2, header.rect.y, header.rect.w - 4, header.title, look.tr, look.tg, look.tb);
        }
    }

    // A name in two lines at most: it breaks at the last space that fits in <maxChars>.
    void SplitName(const std::wstring& name, size_t maxChars, std::wstring& first, std::wstring& second)
    {
        if (name.size() <= maxChars)
        {
            first = name;
            second.clear();
            return;
        }

        size_t cut = name.find_last_of(L' ', maxChars);
        if (cut == std::wstring::npos || cut == 0)
            cut = maxChars;
        first = name.substr(0, cut);
        const size_t next = name.find_first_not_of(L' ', cut);
        second = next == std::wstring::npos ? std::wstring() : Shorten(name.substr(next), maxChars);
    }

    // A big square: the icon on the left; on the right the name (two lines at most), the level and the price.
    void RenderBigCellText(HFONT body, HFONT bold, const Cell& cell, unsigned int gold)
    {
        const int textX = cell.rect.x + BIG_ICON_W + 4;
        const int textW = cell.rect.w - BIG_ICON_W - 6;
        std::wstring first, second;
        SplitName(ItemAttribute[cell.item->Type].Name, BIG_NAME_CHARS, first, second);
        Draw::PutTextLeft(bold, textX, cell.rect.y + 3, textW, first.c_str(), 240, 240, 240);
        if (!second.empty())
            Draw::PutTextLeft(bold, textX, cell.rect.y + 15, textW, second.c_str(), 240, 240, 240);

        wchar_t text[32] = {};
        const int tier = Prices::TierOf(*cell.item);
        const TierLook& look = TIER_LOOK[tier];
        if (cell.item->Level > 0 && tier > 0)
        {
            swprintf(text, std::size(text), L"+%d", cell.item->Level);
            Draw::PutTextLeft(body, textX, cell.rect.y + 28, textW, text, look.tr, look.tg, look.tb);
        }
        else if (cell.item->Durability > 1 && cell.item->Durability <= MAX_SHOWN_QUANTITY)
        {
            swprintf(text, std::size(text), L"x%d", cell.item->Durability);
            Draw::PutTextLeft(body, textX, cell.rect.y + 28, textW, text, 190, 195, 205);
        }

        uint32_t price = 0;
        if (!Prices::TryGetBuyPrice(*cell.item, price))
            return;

        swprintf(text, std::size(text), L"%u", price);
        const bool affordable = gold >= price;
        Draw::PutTextLeft(bold, textX, cell.rect.y + cell.rect.h - 15, textW, text, affordable ? 120 : 255, affordable ? 235 : 110, affordable ? 130 : 110);
    }

    void RenderCellText(HFONT body, HFONT bold, const Cell& cell, unsigned int gold, bool compact)
    {
        if (cell.big)
        {
            RenderBigCellText(body, bold, cell, gold);
            return;
        }

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

        if (tier == 0 && cell.item->Durability > 1 && cell.item->Durability <= MAX_SHOWN_QUANTITY)
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
            const std::wstring name = Shorten(SetNameOf(set.armorType), SET_NAME_CHARS);
            Draw::PutTextCentered(Draw::Heading(), set.rect.x, set.rect.y + 1, set.rect.w, name.c_str(), 240, 235, 215);
            const int role = Prices::SetRoleOf(set.armorType);
            if (role >= 0 && role < ROLE_COUNT)
                Draw::PutTextCentered(Draw::Large(), set.rect.x, set.rect.y + 16, set.rect.w, ROLE_LABEL[role], 160, 190, 235);
            if (set.recommended)
                Draw::PutTextCentered(bold, set.rect.x, set.rect.y + SET_HEADER_H - 12, set.rect.w, L"RECOMENDADO", 255, 214, 80);
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
            Draw::PutTextLeft(body, PANEL_X + PAD + 8, STRIP_Y + 22, 300, L"Clic en un \u00edtem: ver su precio y lo que cambia en tus stats.", 190, 195, 205);
            Draw::PutTextLeft(body, PANEL_X + PAD + 8, STRIP_Y + 36, 300, L"Clic derecho en un \u00edtem: comprarlo al instante.", 190, 195, 205);
            Draw::PutTextLeft(body, PANEL_X + PAD + 8, STRIP_Y + 50, 300, L"Vender: elige el \u00edtem que ya tienes y pulsa Vender.", 190, 195, 205);
            return;
        }

        const int tier = Prices::TierOf(*item);
        const TierLook& look = TIER_LOOK[tier];
        const uint32_t price = PriceOf(*item);
        const bool canBuy = price <= gold;
        wchar_t line[64] = {};
        Draw::PutTextLeft(bold, infoX, STRIP_Y + 3, infoW, Shorten(ItemAttribute[item->Type].Name, STRIP_NAME_CHARS).c_str(), 240, 240, 240);
        if (tier > 0)
            swprintf(line, std::size(line), L"Tier %d%ls", tier, item->Level > 0 ? L"  +" : L"");
        if (tier > 0 && item->Level > 0)
            swprintf(line, std::size(line), L"Tier %d  +%d", tier, static_cast<int>(item->Level));
        if (tier > 0)
            Draw::PutTextLeft(bold, infoX, STRIP_Y + 17, infoW, line, look.tr, look.tg, look.tb);
        const bool isSetPiece = Group(*item) >= FIRST_PIECE_GROUP && Group(*item) < FIRST_PIECE_GROUP + PIECES_PER_SET;
        const int role = isSetPiece
            ? Prices::SetRoleOf(static_cast<uint16_t>(ARMOR_GROUP * CLIENT_GROUP_SIZE + Number(*item)))
            : (tier > 0 && !IsWing(*item) ? Prices::SetRoleOf(item->Type) : -1);   // the accessories carry their own role
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

    // The bar at the right of the catalog when it has more rows than the window shows.
    void RenderScrollbar()
    {
        if (!IsGridTab() || s_maxScroll <= 0 || s_rowTotal <= 0)
            return;

        const int trackY = CONTENT_Y;
        const int trackH = STRIP_Y - 4 - CONTENT_Y;
        const int thumbH = std::max(MIN_THUMB_H, trackH * s_rowShown / s_rowTotal);
        const int thumbY = trackY + (trackH - thumbH) * s_scroll / s_maxScroll;
        Draw::FillRect({ CENTER_X + CENTER_W - SCROLLBAR_W, trackY, SCROLLBAR_W, trackH }, 0.05f, 0.06f, 0.09f, 1.f);
        Draw::FillRect({ CENTER_X + CENTER_W - SCROLLBAR_W, thumbY, SCROLLBAR_W, thumbH }, 0.78f, 0.64f, 0.29f, 1.f);
    }

    // A hint on the first title when there is more below.
    void RenderScrollHint(HFONT body)
    {
        if (!IsGridTab() || s_maxScroll <= 0 || s_scroll != 0 || s_headers.empty())
            return;

        const Rect title = s_headers.front().rect;
        Draw::PutText(body, title.x, title.y, title.w - 2, L"Rueda del mouse: ver m\u00e1s", 170, 175, 190, RT3_SORT_RIGHT);
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
    Layout(store);
    if (MouseWheel != 0 && IsGridTab())
    {
        s_scroll = std::clamp(s_scroll + (MouseWheel > 0 ? -1 : 1), 0, s_maxScroll);
        Layout(store);
    }

    UpdateHover();
    if (clicksEnabled && SEASON3B::IsRelease(VK_LBUTTON))
    {
        HandleClick(store, items, result);
        Layout(store);
    }
    if (clicksEnabled && SEASON3B::IsRelease(VK_RBUTTON))
        HandleRightClick(result);
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
        {
            const IconClip catalogClip(CatalogArea());
            RenderGridBack(gold);
            EndRenderColor();
        }
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
        const IconClip catalogClip(CatalogArea());
        for (const Cell& cell : s_cells)
            RenderCellText(body, bold, cell, gold, false);
        RenderSectionTitles(bold);
        RenderScrollHint(body);
    }

    RenderDetailText(body, bold, DetailItem(), gold);
    Draw::PutTextLeft(body, panel.x + PAD, panel.y + panel.h - 16, panel.w - 2 * PAD,
        L"Clic: ver detalle.  Clic derecho: comprar.  La joya de perfecci\u00f3n se arrastra sobre un \u00edtem de tu inventario.", 170, 175, 190);

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
        RenderIcon(cell.item->Type, cell.item->Level, cell.item->ExcellentFlags, cell.item->AncientDiscriminator, IconBoxOf(cell), IconShiftOf(cell), IsSetsTab() ? REFERENCE_HEIGHT : CatalogArea().y + CatalogArea().h);
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
