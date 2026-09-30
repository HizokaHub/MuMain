#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct tagITEM;

// MOBA shop prices, pushed by the server (packet C2 D5 08) whenever the MOBA shop
// window opens. The server owns the price formula; the client only looks prices up
// by item properties (type, level, option level, luck, excellent count) so a price
// follows the item between the shop grid, the inventory and the equipment.
namespace GameLogic::Items::MobaShopPrices
{
    struct Entry
    {
        uint16_t Type;
        uint8_t Level;
        uint8_t OptionLevel;
        bool HasLuck;
        uint8_t ExcellentCount;
        bool PerUnit;      // price is per unit of a stack (multiplied by the durability)
        uint32_t Price;    // buy price
    };

    // MOBA-only option of an item (anti-heal, crowd-control resistance, cooldown reduction, assist / passive
    // gold), pushed by the server (packet C2 D5 0A) and shown as one extra tooltip line. Kind: 1 anti-heal,
    // 2 CC resistance, 3 cooldown reduction, 4 assist gold, 5 passive gold.
    struct Trait
    {
        uint16_t Type;
        uint8_t Kind;
        uint8_t Percent;
    };

    void SetTraits(std::vector<Trait> traits);

    // MOBA options of one kind of shield / book (packet C2 D5 0C). They can differ per item (shields dropped by
    // creeps roll 1-2 of four), so the item is identified like a price: type, level, option level, luck and
    // excellent count. Each option is (kind, percent), kinds as in Trait.
    struct ShieldOptions
    {
        uint16_t Type;
        uint8_t Level;
        uint8_t OptionLevel;
        bool HasLuck;
        uint8_t ExcellentCount;
        std::vector<std::pair<uint8_t, uint8_t>> Options;
    };

    void SetShieldOptions(std::vector<ShieldOptions> shields);

    // The tooltip lines of the shield options of the item (empty if it has none).
    std::vector<std::wstring> GetShieldOptionTexts(const tagITEM& item);

    // The tooltip line of the item's trait (e.g. "Anti-curacion 20%"), if it has one.
    bool TryGetTraitText(uint16_t itemType, wchar_t* text, size_t textLength);

    // Replaces the whole price table.
    void Set(std::vector<Entry> entries, uint8_t sellPercent);

    // Forgets all prices (the shop window closed).
    void Clear();

    // Price to buy the item from the MOBA shop.
    bool TryGetBuyPrice(const tagITEM& item, uint32_t& price);

    // Price the MOBA shop pays back when the item is sold.
    bool TryGetSellPrice(const tagITEM& item, uint32_t& price);
}
