#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The row of badges to the right of the strip of the Elf's creature: one per extra pet the own champion has switched on from the inventory (the mask the
    // server sends in C1 D5 2C, see World::Moba::ExtraPets). Hovering a badge names the pet. Only drawn in the arena and only while some pet is on.
    class CMobaPetHud : public CNewUIObj
    {
    public:
        // Registers the row in the manager (once). The key sits past the INTERFACE_ enum so no shared header changes.
        static void Create(CNewUIManager* pNewUIMng);

        bool Render() override;
        bool Update() override;
        bool UpdateMouseEvent() override;
        bool UpdateKeyEvent() override;

        float GetLayerDepth() override;
        float GetKeyEventOrder() override;
    };
}
