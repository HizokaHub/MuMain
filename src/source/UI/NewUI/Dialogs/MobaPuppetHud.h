#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The HUD of the control of a disconnected teammate: a card with the portrait, the name, the life and the mana of every teammate without player (a click takes
    // it, like a tap of Ctrl), and, while one is controlled, its skill bar with the rank and the cooldown of each skill (the keys 1-9 cast them; a click on a skill
    // and then a number key moves it to that key). The state comes from World::Moba::Puppet.
    class CMobaPuppetHud : public CNewUIObj
    {
    public:
        // Registers the HUD in the manager (once). The key sits past the INTERFACE_ enum so no shared header changes.
        static void Create(CNewUIManager* pNewUIMng);

        bool Render() override;
        bool Update() override;
        bool UpdateMouseEvent() override;
        bool UpdateKeyEvent() override;

        float GetLayerDepth() override;
        float GetKeyEventOrder() override;
    };
}
