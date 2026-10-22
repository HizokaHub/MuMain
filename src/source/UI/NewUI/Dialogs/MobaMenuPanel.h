#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The menu panel of the NPCs of the MOBA mode (queue master, private rooms, shop categories): the title, the text and a
    // button per option in the style of the loadout window, in larger letters than the native NPC dialogue. An option which
    // asks for a room code turns the panel into a box where the code is typed with the keyboard. The state comes from
    // World::Moba::Menu.
    class CMobaMenuPanel : public CNewUIObj
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
