#pragma once

#include <cstdint>
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

    // Replaces the whole price table.
    void Set(std::vector<Entry> entries, uint8_t sellPercent);

    // Forgets all prices (the shop window closed).
    void Clear();

    // Price to buy the item from the MOBA shop.
    bool TryGetBuyPrice(const tagITEM& item, uint32_t& price);

    // Price the MOBA shop pays back when the item is sold.
    bool TryGetSellPrice(const tagITEM& item, uint32_t& price);
}
