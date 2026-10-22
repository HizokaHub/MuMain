#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The panel of a MOBA private room, on the left of the screen while the player is in one: the code in big letters, the
    // format, both teams with their players and the buttons (change team, bots, start for the host, leave). The state comes
    // from World::Moba::Room.
    class CMobaRoomPanel : public CNewUIObj
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
