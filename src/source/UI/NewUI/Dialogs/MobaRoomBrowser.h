#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The room browser of the MOBA private rooms, opened from the queue master: the list of the open rooms (name, host, format,
    // players, how long ago it was created; a join button each) and the form to create one (name, players per team, map, bots).
    // The state comes from World::Moba::RoomList.
    class CMobaRoomBrowser : public CNewUIObj
    {
    public:
        // Registers the browser in the manager (once). The key sits past the INTERFACE_ enum so no shared header changes.
        static void Create(CNewUIManager* pNewUIMng);

        bool Render() override;
        bool Update() override;
        bool UpdateMouseEvent() override;
        bool UpdateKeyEvent() override;

        float GetLayerDepth() override;
        float GetKeyEventOrder() override;
    };
}
