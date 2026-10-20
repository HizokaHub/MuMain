#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The "match found" popup of the MOBA queue: a panel in the upper middle of the screen with the countdown (text and bar),
    // one square per player of the match (lit once they accepted) and the Accept / Decline buttons. Enter accepts; declining
    // is the button only (Esc is not bound: a decline earns a warning, so it must not happen by habit). The state comes from
    // World::Moba::ReadyCheck.
    class CMobaReadyCheckWindow : public CNewUIObj
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
