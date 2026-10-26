#include "stdafx.h"
#include "World/Moba/MobaExtraPets.h"

#include "Engine/AI/GOBoid.h"
#include "Engine/Object/ZzzObject.h"
#include "GameLogic/Pets/GIPetManager.h"
#include "GameLogic/Pets/w_PetProcess.h"
#include "World/MapInfra/MapManager.h"

namespace World::Moba::ExtraPets
{

namespace
{
    constexpr int BIT_COUNT = PET_COUNT;
    constexpr int BIT_ANGEL = 0;
    constexpr int BIT_IMP = 1;
    constexpr int BIT_RAVEN = 8;

    // Item number (group 13) of the pets that are drawn by the pet system (bits 2..7).
    constexpr int PET_ITEM_NUMBER[BIT_COUNT] = { 0, 1, 64, 65, 67, 80, 106, 123, 0 };

    std::vector<std::pair<unsigned short, unsigned short>> s_pets;

    unsigned short DesiredMask(unsigned short key)
    {
        for (const auto& entry : s_pets)
        {
            if (entry.first == key)
                return entry.second;
        }
        return 0;
    }

    int MountModel(int bit)
    {
        return bit == BIT_ANGEL ? MODEL_HELPER : MODEL_IMP;
    }

    OBJECT* FindMount(OBJECT* owner, int model)
    {
        for (int i = 0; i < MAX_MOUNTS; ++i)
        {
            OBJECT* mount = &Mounts[i];
            if (mount->Live && mount->Owner == owner && mount->Type == model)
                return mount;
        }
        return nullptr;
    }

    bool Exists(CHARACTER* c, int bit)
    {
        if (bit == BIT_ANGEL || bit == BIT_IMP)
            return FindMount(&c->Object, MountModel(bit)) != nullptr;
        if (bit == BIT_RAVEN)
            return c->m_pPet != nullptr;
        return ThePetProcess().HasPet(c, ITEM_HELPER + PET_ITEM_NUMBER[bit]);
    }

    void Create(CHARACTER* c, int bit)
    {
        OBJECT* o = &c->Object;
        if (bit == BIT_ANGEL || bit == BIT_IMP)
        {
            CreateMount(MountModel(bit), o->Position, o);
        }
        else if (bit == BIT_RAVEN)
        {
            giPetManager::CreatePetDarkSpirit(c);
        }
        else
        {
            ThePetProcess().CreatePet(ITEM_HELPER + PET_ITEM_NUMBER[bit], MODEL_HELPER + PET_ITEM_NUMBER[bit], o->Position, c);
        }
    }

    void Remove(CHARACTER* c, int bit)
    {
        if (bit == BIT_ANGEL || bit == BIT_IMP)
        {
            if (OBJECT* mount = FindMount(&c->Object, MountModel(bit)))
                mount->Live = false;
        }
        else if (bit == BIT_RAVEN)
        {
            giPetManager::DeletePet(c);
        }
        else
        {
            ThePetProcess().DeletePet(c, ITEM_HELPER + PET_ITEM_NUMBER[bit], false);
        }
    }
}

unsigned short MaskOf(unsigned short key)
{
    return DesiredMask(key);
}

void SetAll(const std::vector<std::pair<unsigned short, unsigned short>>& pets)
{
    s_pets = pets;
}

void Apply(CHARACTER* c)
{
    if (c == nullptr || !c->Object.Live || c->Object.Kind != KIND_PLAYER)
        return;

    const bool inArena = gMapManager.WorldActive == WD_200_MOBA_ARENA;
    const unsigned short desired = inArena ? DesiredMask(static_cast<unsigned short>(c->Key)) : 0;
    if (desired == 0 && c->MobaExtraMask == 0)
        return;

    for (int bit = 0; bit < BIT_COUNT; ++bit)
    {
        const unsigned short flag = static_cast<unsigned short>(1 << bit);
        const bool wanted = (desired & flag) != 0;
        const bool ours = (c->MobaExtraMask & flag) != 0;
        if (wanted)
        {
            if (!Exists(c, bit))
            {
                Create(c, bit);
                c->MobaExtraMask |= flag;
            }
        }
        else if (ours)
        {
            // Only what this module made is removed (a pet the character wears for real stays).
            if (Exists(c, bit))
                Remove(c, bit);
            c->MobaExtraMask &= static_cast<unsigned short>(~flag);
        }
    }
}

}
