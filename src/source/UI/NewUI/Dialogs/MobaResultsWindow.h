#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The results table at the end of a MOBA match: victory or defeat, the duration, one line per champion (class, level,
    // K/D/A, minions, gold, damage dealt and taken; the player's own line highlighted) and the button which sends the player
    // back to Lorencia. The state comes from World::Moba::Results.
    class CMobaResultsWindow : public CNewUIObj
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
