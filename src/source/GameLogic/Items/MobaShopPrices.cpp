#include "stdafx.h"
#include "GameLogic/Items/MobaShopPrices.h"

namespace
{
    constexpr uint8_t ExcellentOptionMask = 0x3F; // bits 6/7 carry non-excellent flags (Fenrir)
    constexpr uint32_t PercentBase = 100;

    std::vector<GameLogic::Items::MobaShopPrices::Entry> s_entries;
    std::vector<GameLogic::Items::MobaShopPrices::Trait> s_traits;
    std::vector<GameLogic::Items::MobaShopPrices::ShieldOptions> s_shields;
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
    void SetTraits(std::vector<Trait> traits)
    {
        s_traits = std::move(traits);
    }

    void SetShieldOptions(std::vector<ShieldOptions> shields)
    {
        s_shields = std::move(shields);
    }

    std::vector<std::wstring> GetShieldOptionTexts(const tagITEM& item)
    {
        std::vector<std::wstring> lines;
        const int excellentCount = CountExcellentOptions(item.ExcellentFlags);
        for (const auto& shield : s_shields)
        {
            if (shield.Type != item.Type || shield.Level != item.Level || shield.OptionLevel != item.OptionLevel
                || shield.HasLuck != item.HasLuck || shield.ExcellentCount != excellentCount)
                continue;

            for (const auto& [kind, percent] : shield.Options)
            {
                const wchar_t* format = nullptr;
                switch (kind)
                {
                case 2: format = L"Resistencia a control: -%d%% duracion del control"; break;
                case 3: format = L"Reduccion de enfriamiento: -%d%%"; break;
                case 4: format = L"Oro por asistencia: +%d%%"; break;
                case 5: format = L"Oro pasivo: +%d%%"; break;
                default: continue;
                }

                wchar_t text[100];
                swprintf(text, 100, format, (int)percent);
                lines.emplace_back(text);
            }

            break;
        }

        return lines;
    }

    bool TryGetTraitText(uint16_t itemType, wchar_t* text, size_t textLength)
    {
        for (const auto& trait : s_traits)
        {
            if (trait.Type != itemType)
                continue;

            const wchar_t* format = nullptr;
            switch (trait.Kind)
            {
            case 1: format = L"Anti-curacion: los golpes reducen %d%% la curacion del objetivo (3 s)"; break;
            case 2: format = L"Resistencia a control: -%d%% duracion del control"; break;
            case 3: format = L"Reduccion de enfriamiento: -%d%%"; break;
            case 4: format = L"Oro por asistencia: +%d%%"; break;
            case 5: format = L"Oro pasivo: +%d%%"; break;
            default: return false;
            }

            swprintf(text, textLength, format, (int)trait.Percent);
            return true;
        }

        return false;
    }

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
