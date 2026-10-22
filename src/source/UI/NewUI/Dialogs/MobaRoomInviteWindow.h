#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The invitation popup of the MOBA private rooms: a small panel at the top of the screen with who invites, the room and the
    // time left, and the Accept / Decline buttons. The state comes from World::Moba::Room (CurrentInvite).
    class CMobaRoomInviteWindow : public CNewUIObj
    {
    public:
        // Registers the popup in the manager (once). The key sits past the INTERFACE_ enum so no shared header changes.
        static void Create(CNewUIManager* pNewUIMng);

        bool Render() override;
        bool Update() override;
        bool UpdateMouseEvent() override;
        bool UpdateKeyEvent() override;

        float GetLayerDepth() override;
        float GetKeyEventOrder() override;
    };
}
