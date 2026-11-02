#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The commander console of the MOBA commanders mode (Direct Strike style): an essence bar at the top of the screen (with a tick at
    // the cost of each pack) and the timer of the next wave under it, for everyone. The commander also gets a full-width console at
    // the bottom (the RTS view, see World::Moba::Commander::RtsView): the minimap at the left (it moves there by itself), a card per
    // lane in the middle (creeps of each team, amber when the enemy presses it, and the packs waiting in the base by type) and a 3x3
    // command card at the right, plus "give up the command". Keys: 1/2/3 pick the lane, F1/F2/F3 point the camera at it, Q/W/E buy a
    // pack for it (Shift = x5; the creatures wait in the base until the timer releases the wave), A buys income, X sends the waiting
    // creatures now; S, D, Z and C are reserved. A player of a team commanded by a bot sees a button to take the command. The state
    // comes from World::Moba::Commander and it only shows while a commanders match runs.
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
