#include "stdafx.h"
#include "GameLogic/Items/MobaShopPrices.h"

namespace
{
    constexpr uint8_t ExcellentOptionMask = 0x3F; // bits 6/7 carry non-excellent flags (Fenrir)
    constexpr uint32_t PercentBase = 100;

    std::vector<GameLogic::Items::MobaShopPrices::Entry> s_entries;
    std::vector<GameLogic::Items::MobaShopPrices::Trait> s_traits;
    std::vector<GameLogic::Items::MobaShopPrices::ShieldOptions> s_shields;
    std::vector<GameLogic::Items::MobaShopPrices::Recommended> s_recommended;
    std::vector<GameLogic::Items::MobaShopPrices::SetRole> s_setRoles;
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
    bool HasPrices()
    {
        return !s_entries.empty();
    }

    int TierOf(const tagITEM& item)
    {
        const Entry* entry = Find(item);
        if (entry == nullptr || entry->ExcellentCount < 3)
            return 0;
        return entry->ExcellentCount >= 5 ? 3 : (entry->ExcellentCount >= 4 ? 2 : 1);
    }

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

    namespace
    {
        constexpr uint8_t LAST_TIER_KIND = 5;        // kinds 1-5 carry whole percents; the archetype kinds (6 and up) carry tenths of a percent
        constexpr int TENTHS_PER_PERCENT = 10;
        constexpr uint8_t GAUGE_REGEN_KIND = 14;      // AG regeneration of the accessories (whole percent)

        const wchar_t* TierTraitFormat(uint8_t kind)
        {
            switch (kind)
            {
            case 1: return L"Anti-curacion: los golpes reducen %d%% la curacion del objetivo (3 s)";
            case 2: return L"Resistencia a control: -%d%% duracion del control";
            case 3: return L"Reduccion de enfriamiento: -%d%%";
            case 4: return L"Oro por asistencia: +%d%%";
            case 5: return L"Oro pasivo: +%d%%";
            case GAUGE_REGEN_KIND: return L"Regeneracion de AG: +%d%% (teletransporte del Soul Master)";
            default: return nullptr;
            }
        }

        // The archetype bonuses of the pieces (what a set gives to the class it is sold to); the value is in tenths of a percent.
        const wchar_t* ArchetypeTraitFormat(uint8_t kind)
        {
            switch (kind)
            {
            case 6: return L"Vida maxima: +%d.%d%%";
            case 7: return L"Durabilidad: -%d.%d%% al dano recibido";
            case 8: return L"Mana maximo: +%d.%d%%";
            case 9: return L"Regeneracion de mana: +%d.%d%%";
            case 10: return L"Potencia de habilidades: +%d.%d%%";
            case 11: return L"Potencia de curacion y escudos: +%d.%d%%";
            case 12: return L"Probabilidad de critico: +%d.%d%%";
            case 13: return L"Regeneracion de vida: +%d.%d%% de la vida por segundo";
            default: return nullptr;
            }
        }
    }

    std::vector<std::wstring> GetTraitTexts(uint16_t itemType)
    {
        std::vector<std::wstring> lines;
        for (const auto& trait : s_traits)
        {
            if (trait.Type != itemType)
                continue;

            wchar_t text[120];
            if (trait.Kind <= LAST_TIER_KIND || trait.Kind == GAUGE_REGEN_KIND)
            {
                const wchar_t* format = TierTraitFormat(trait.Kind);
                if (format == nullptr)
                    continue;

                swprintf(text, std::size(text), format, (int)trait.Percent);
            }
            else
            {
                const wchar_t* format = ArchetypeTraitFormat(trait.Kind);
                if (format == nullptr)
                    continue;

                swprintf(text, std::size(text), format, (int)trait.Percent / TENTHS_PER_PERCENT, (int)trait.Percent % TENTHS_PER_PERCENT);
            }

            lines.emplace_back(text);
        }

        return lines;
    }

    void Set(std::vector<Entry> entries, uint8_t sellPercent)
    {
        s_entries = std::move(entries);
        s_sellPercent = sellPercent;
    }

    void SetRecommended(std::vector<Recommended> steps, std::vector<SetRole> roles)
    {
        s_recommended = std::move(steps);
        s_setRoles = std::move(roles);
    }

    const std::vector<Recommended>& GetRecommended()
    {
        return s_recommended;
    }

    int SetRoleOf(uint16_t armorType)
    {
        for (const SetRole& role : s_setRoles)
        {
            if (role.Type == armorType)
                return role.Variant;
        }
        return -1;
    }

    void Clear()
    {
        s_entries.clear();
        s_recommended.clear();
        s_setRoles.clear();
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
