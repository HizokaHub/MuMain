#include "stdafx.h"
#include "GameLogic/Items/MobaShopPrices.h"

namespace
{
    constexpr uint8_t ExcellentOptionMask = 0x3F; // bits 6/7 carry non-excellent flags (Fenrir)
    constexpr uint32_t PercentBase = 100;

    std::vector<GameLogic::Items::MobaShopPrices::Entry> s_entries;
    uint8_t s_sellPercent = 0;

    int CountExcellentOptions(BYTE excellentFlags)
    {
        int count = 0;
        for (BYTE bits = excellentFlags & ExcellentOptionMask; bits != 0; bits &= bits - 1)
        {
            ++count;
        }

        return count;
    }

    const GameLogic::Items::MobaShopPrices::Entry* Find(const ITEM& item)
    {
        const int excellentCount = CountExcellentOptions(item.ExcellentFlags);
        for (const auto& entry : s_entries)
        {
            if (entry.Type == item.Type
                && entry.Level == item.Level
                && entry.OptionLevel == item.OptionLevel
                && entry.HasLuck == item.HasLuck
                && entry.ExcellentCount == excellentCount)
            {
                return &entry;
            }
        }

        return nullptr;
    }

    uint32_t TotalPrice(const GameLogic::Items::MobaShopPrices::Entry& entry, const ITEM& item)
    {
        return entry.PerUnit ? entry.Price * item.Durability : entry.Price;
    }
}

namespace GameLogic::Items::MobaShopPrices
{
    void Set(std::vector<Entry> entries, uint8_t sellPercent)
    {
        s_entries = std::move(entries);
        s_sellPercent = sellPercent;
    }

    void Clear()
    {
        s_entries.clear();
        s_sellPercent = 0;
    }

    bool TryGetBuyPrice(const tagITEM& item, uint32_t& price)
    {
        const Entry* entry = Find(item);
        if (entry == nullptr)
            return false;

        price = TotalPrice(*entry, item);
        return true;
    }

    bool TryGetSellPrice(const tagITEM& item, uint32_t& price)
    {
        const Entry* entry = Find(item);
        if (entry == nullptr)
            return false;

        price = TotalPrice(*entry, item) * s_sellPercent / PercentBase;
        return true;
    }
}
