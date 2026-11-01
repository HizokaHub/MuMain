#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The commander console of the MOBA commanders mode (RTS style): an essence bar at the top of the screen (with a tick at the
    // cost of each pack) for everyone, and for the commander a console above the HUD with a card per lane (creeps of each team,
    // amber when the enemy presses it), a button per pack plus the income upgrade, and "give up the command". Keys: 1/2/3 pick the
    // lane, Q/W/E send a pack down it (Shift = x5), R buys income. A player of a team commanded by a bot sees a button to take the
    // command. The state comes from World::Moba::Commander and it only shows while a commanders match runs.
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
