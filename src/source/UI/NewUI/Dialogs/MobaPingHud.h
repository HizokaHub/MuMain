#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The ping gesture and the marks of the pings on the ground (World::Moba::Ping): hold G / Alt (a plain ping) or V / Ctrl (caution) and click; keep the
    // button down and the wheel of nine pings opens around the click (drag toward one and let go, the right button closes it). The pings of the team show on
    // the ground as an icon with an expanding ring, and as an arrow at the edge of the screen when they are out of view. The minimap draws its own marks.
    class CMobaPingHud : public CNewUIObj
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
