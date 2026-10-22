#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The loadout window of the MOBA mode: a grid with the icon of every skill the player's class can take into a match;
    // a click picks or drops a skill (between the minimum and maximum the server says), Save sends the pick, Esc cancels.
    // The state comes from World::Moba::Loadout.
    class CMobaLoadoutWindow : public CNewUIObj
    {
    public:
        // Registers the window in the manager (once). The key sits past the INTERFACE_ enum so no shared header changes.
        static void Create(CNewUIManager* pNewUIMng);

        bool Render() override;
        bool Update() override;
        bool UpdateMouseEvent() override;
        bool UpdateKeyEvent() override;

        float GetLayerDepth() override;
        float GetKeyEventOrder() override;
    };
}
