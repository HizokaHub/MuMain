#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The commander panel of the MOBA commanders mode: a panel on the left of the screen with the essence of the team and its
    // income. The commander also sees one row of buttons per pack of creeps (a button per lane, grayed while the lane is closed
    // or the essence short), the income upgrade and "give up the command"; a player of a team commanded by a bot sees a button to
    // take the command. The state comes from World::Moba::Commander and the panel only shows while a commanders match runs.
    class CMobaCommanderWindow : public CNewUIObj
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
