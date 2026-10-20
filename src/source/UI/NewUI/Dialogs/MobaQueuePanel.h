#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The queue panel of the MOBA: while the player searches for a match it shows, in the upper right corner, how long they have
    // waited and how many wait with them and, for the search with bots, the steps of the wait (after how long a match with bots
    // starts with how many humans) with the current step marked. A cancel button leaves the queue. The state comes from
    // World::Moba::QueueStatus.
    class CMobaQueuePanel : public CNewUIObj
    {
    public:
        // Registers the panel in the manager (once). The key sits past the INTERFACE_ enum so no shared header changes.
        static void Create(CNewUIManager* pNewUIMng);

        bool Render() override;
        bool Update() override;
        bool UpdateMouseEvent() override;
        bool UpdateKeyEvent() override;

        float GetLayerDepth() override;
        float GetKeyEventOrder() override;
    };
}
