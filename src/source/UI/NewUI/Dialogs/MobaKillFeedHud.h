#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The kill feed in the upper right corner: the last kills, towers and objectives of the match as short lines in the colour of the team that did them
    // (the data is World::Moba::Announcer). It also gives the announcer its frame tick, so a waiting voice line starts when the current one ends.
    class CMobaKillFeedHud : public CNewUIObj
    {
    public:
        // Registers the feed in the manager (once). The key sits past the INTERFACE_ enum so no shared header changes.
        static void Create(CNewUIManager* pNewUIMng);

        bool Render() override;
        bool Update() override;
        bool UpdateMouseEvent() override;
        bool UpdateKeyEvent() override;

        float GetLayerDepth() override;
        float GetKeyEventOrder() override;
    };
}
