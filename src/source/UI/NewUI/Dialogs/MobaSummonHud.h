#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The strip above the skill bar with the Elf's creature: its life, the rank of the skill and whether the player commands it
    // (key X, or a click on the strip). The state comes from World::Moba::Summon.
    class CMobaSummonHud : public CNewUIObj
    {
    public:
        // Registers the strip in the manager (once). The key sits past the INTERFACE_ enum so no shared header changes.
        static void Create(CNewUIManager* pNewUIMng);

        bool Render() override;
        bool Update() override;
        bool UpdateMouseEvent() override;
        bool UpdateKeyEvent() override;

        float GetLayerDepth() override;
        float GetKeyEventOrder() override;
    };
}
