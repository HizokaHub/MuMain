//////////////////////////////////////////////////////////////////////
// NewUIGuildInfoWindow.h: interface for the CNewUIGuildInfoWindow class.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "UI/NewUI/NewUIBase.h"
#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/HUD/NewUIMainFrameWindow.h"
#include "UI/NewUI/HUD/NewUIChatLogWindow.h"
#include "UI/NewUI/Inventory/NewUIMyInventory.h"

namespace SEASON3B
{
    class CNewUIMiniMap : public CNewUIObj
    {
    public:
        enum IMAGE_LIST
        {
            IMAGE_MINIMAP_INTERFACE = BITMAP_MINI_MAP_BEGIN,
            // MOBA corner-minimap atlases (Data\Interface\MobaPortraits.OZT, MobaMinimapIcons.OZT).
            IMAGE_MINIMAP_MOBA_PORTRAITS = IMAGE_MINIMAP_INTERFACE + 7,
            IMAGE_MINIMAP_MOBA_ICONS = IMAGE_MINIMAP_INTERFACE + 8,
            // The arena's own ground and roads (Data\World200\MobaMinimapArt.OZT, tools/make_minimap_art.py), optional.
            IMAGE_MINIMAP_MOBA_ART = IMAGE_MINIMAP_INTERFACE + 9,
        };

        enum MASTER_DATA
        {
            SKILL_ICON_DATA_WDITH = 4,
            SKILL_ICON_DATA_HEIGHT = 8,
            SKILL_ICON_WIDTH = 20,
            SKILL_ICON_HEIGHT = 28,
            SKILL_ICON_STARTX1 = 75,
            SKILL_ICON_STARTY1 = 75,
        };

        enum EVENT_STATE
        {
            EVENT_NONE = 0,
            EVENT_SCROLL_BTN_DOWN,
        };

    private:
       std::wstring		m_TooltipText;
        HFONT					m_hToolTipFont;
        DWORD					m_TooltipTextColor;

        CNewUIManager* m_pNewUIMng;
        POINT					m_Pos;
        POINT					m_Width;
        POINT					m_MiniWidth;
        POINT					m_Lenth[6];
        int						m_MiniPos;
        CNewUIButton			m_BtnExit;
        MINI_MAP				m_Mini_Map_Data[MAX_MINI_MAP_DATA];
        float					m_Btn_Loc[MAX_MINI_MAP_DATA][4];

    public:
        bool					m_bSuccess;
        CNewUIMiniMap();
        virtual ~CNewUIMiniMap();

        bool Create(CNewUIManager* pNewUIMng, int x, int y);
        void Release();

        void SetPos(int x, int y);
        void SetBtnPos(int Num, float x, float y, float nx, float ny);

        bool UpdateMouseEvent();
        bool UpdateKeyEvent();
        bool Update();
        bool Render();

        // Always-on small minimap pinned to the bottom-right corner (LoL style).
        // The caller decides when to show it (currently only on the Crywolf map).
        void RenderCornerMinimap();

        // Options gear + panel of the corner minimap (size, opacity, markers, corner).
        // Returns true when the mouse was used by it, so the click does not reach the world.
        bool UpdateCornerMouse();

        // True while the corner minimap is in the middle of something which must keep the mouse even
        // under other windows: the options panel is open, a slider or the size handle is being dragged,
        // or a click was just used. Otherwise any open window is served before the minimap.
        bool CornerMouseBusy() const;

        // Wraps RenderCornerMinimap with the "on the MOBA arena and the full Tab
        // map isn't open" gate. Called from the UI manager's render pass at a low
        // z so any window the player opens (stats, inventory, skill list, ...)
        // draws over it instead of the other way round.
        void RenderCornerOverlayIfActive();

        float GetLayerDepth();	//. 8.1f

        void OpenningProcess();
        void ClosingProcess();
        void OpenMasterLevel(const wchar_t* filename);
        CNewUIButton m_BtnToolTip;
        void LoadImages(const wchar_t* Filename);
        void UnloadImages();

    private:
        bool m_bOptionsOpen = false;
        bool m_bSwallowHold = false;
        int m_iDragSlider = -1;   // options slider being dragged, -1 = none
        // Whether the MOBA marker atlases loaded (optional files: the markers fall back to flat squares).
        bool m_bMobaPortraitsLoaded = false;
        bool m_bMobaIconsLoaded = false;
        void RenderCornerOptions();
        void Render_Text();
        void Render_Icon();
        void Render_Scroll();
        bool Check_Mouse(int mx, int my);
        bool Check_Btn(int mx, int my);
    };
}
