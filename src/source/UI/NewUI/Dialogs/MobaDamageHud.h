#pragma once

#include "UI/NewUI/NewUIBase.h"

namespace SEASON3B
{
    class CNewUIManager;

    // The damage the champion takes, with direction: a red wedge around the champion that points at the attacker (blue when only the shield took it);
    // damage without a source (poison, burning) pulses a red ring. The stronger the hit, the brighter the wedge. The data is World::Moba::DamageDir.
    class CMobaDamageHud : public CNewUIObj
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
