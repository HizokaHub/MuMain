// NewUIGuildInfoWindow.cpp: implementation of the CNewUIGuildInfoWindow class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "I18N/All.h"

#include "UI/NewUI/HUD/NewUIMiniMap.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Dialogs/NewUICommonMessageBox.h"
#include "UI/NewUI/Dialogs/NewUICustomMessageBox.h"
#include "Audio/DSPlaySound.h"

#include "Guild/NewUIGuildInfoWindow.h"
#include "UI/NewUI/Widgets/NewUIButton.h"
#include "UI/NewUI/Inventory/NewUIMyInventory.h"
#include "GameLogic/Items/CSItemOption.h"
#include "World/MapInfra/MapManager.h"
#include "Render/Terrain/ZzzLodTerrain.h" // TerrainWall (MOBA wall overlay)
#include "Network/Server/WSclient.h" // g_MobaMinimap (MOBA minimap feed)
#include "Data/GameConfig/GameConfig.h" // corner minimap options
#include <algorithm>
#include <vector>
#include "Render/RHI/RHI.h"
#include "Render/Core/BindState.h"
#include "Render/Sprites/GlobalBitmap.h"

extern BYTE m_OccupationState;
float ConvertX(float x);
float ConvertY(float y);

using namespace SEASON3B;

SEASON3B::CNewUIMiniMap::CNewUIMiniMap()
{
    m_pNewUIMng = NULL;
}

SEASON3B::CNewUIMiniMap::~CNewUIMiniMap()
{
    Release();
}

bool SEASON3B::CNewUIMiniMap::Create(CNewUIManager* pNewUIMng, int x, int y)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_MINI_MAP, this);

    LoadBitmap(L"Interface\\mini_map_ui_corner.tga", IMAGE_MINIMAP_INTERFACE + 1, GL_LINEAR);
    LoadBitmap(L"Interface\\mini_map_ui_line.jpg", IMAGE_MINIMAP_INTERFACE + 2, GL_LINEAR);
    LoadBitmap(L"Interface\\mini_map_ui_cha.tga", IMAGE_MINIMAP_INTERFACE + 3, GL_LINEAR);
    LoadBitmap(L"Interface\\mini_map_ui_portal.tga", IMAGE_MINIMAP_INTERFACE + 4, GL_LINEAR);
    LoadBitmap(L"Interface\\mini_map_ui_npc.tga", IMAGE_MINIMAP_INTERFACE + 5, GL_LINEAR);
    LoadBitmap(L"Interface\\mini_map_ui_cancel.tga", IMAGE_MINIMAP_INTERFACE + 6, GL_LINEAR);

    m_BtnExit.ChangeButtonImgState(true, IMAGE_MINIMAP_INTERFACE + 6, false);
    m_BtnExit.ChangeButtonInfo(m_Pos.x + 610, 3, 85, 85);
    m_BtnExit.ChangeToolTipText(&I18N::Game::Close388, true);	// 1002 "�ݱ�"

    SetPos(x, y);

    m_Lenth[0].x = 800;
    m_Lenth[1].x = 1000;
    m_Lenth[2].x = 1200;
    m_Lenth[3].x = 1400;
    m_Lenth[4].x = 1600;
    m_Lenth[5].x = 1800;
    m_Lenth[0].y = 800;
    m_Lenth[1].y = 1000;
    m_Lenth[2].y = 1200;
    m_Lenth[3].y = 1400;
    m_Lenth[4].y = 1600;
    m_Lenth[5].y = 1800;
    m_MiniPos = 0;
    m_bSuccess = false;
    return true;
}

void SEASON3B::CNewUIMiniMap::ClosingProcess()
{
    SocketClient->ToGameServer()->SendCloseNpcRequest();
}

float SEASON3B::CNewUIMiniMap::GetLayerDepth()
{
    return 8.1f;
}

void SEASON3B::CNewUIMiniMap::OpenningProcess()
{
}

void SEASON3B::CNewUIMiniMap::Release()
{
    UnloadImages();

    for (int i = 1; i < 7; i++)
    {
        DeleteBitmap(IMAGE_MINIMAP_INTERFACE + i);
    }

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }
}

void SEASON3B::CNewUIMiniMap::SetPos(int x, int y)
{
    m_BtnExit.ChangeButtonInfo(REFERENCE_WIDTH - 27, 3, 30, 25);
}

void SEASON3B::CNewUIMiniMap::SetBtnPos(int Num, float x, float y, float nx, float ny)
{
    m_Btn_Loc[Num][0] = x;
    m_Btn_Loc[Num][1] = y;
    m_Btn_Loc[Num][2] = nx;
    m_Btn_Loc[Num][3] = ny;
}

bool SEASON3B::CNewUIMiniMap::UpdateKeyEvent()
{
    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MINI_MAP))
    {
        if (IsPress(VK_ESCAPE) == true || IsPress(VK_TAB) == true)
        {
            g_pNewUISystem->Hide(SEASON3B::INTERFACE_MINI_MAP);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }
    return true;
}

namespace
{
    // MOBA minimap feed: team colour of an entry (wards are green), and its size on the small / full map.
    void SetMobaMarkerColor(const MobaMinimapEntry& e)
    {
        if (e.kind == 4)
            glColor4f(0.35f, 1.f, 0.35f, 1.f);
        else if (e.team == 1)
            glColor4f(0.35f, 0.6f, 1.f, 1.f);
        else
            glColor4f(1.f, 0.35f, 0.35f, 1.f);
    }

    float MobaMarkerSize(const MobaMinimapEntry& e, float champion, float structure, float nexus, float ward)
    {
        return e.kind == 1 ? champion : e.kind == 2 ? structure : e.kind == 3 ? nexus : ward;
    }
}

bool SEASON3B::CNewUIMiniMap::Render()
{
    float Rot = 45.f;

    if (m_bSuccess == false)
        return m_bSuccess;

    EnableAlphaTest();
    RenderColor(0, 0, REFERENCE_WIDTH, 430, 0.85f, 1);
    DisableAlphaBlend();
    EnableAlphaTest();
    glColor4f(1.f, 1.f, 1.f, 1.f);

    // Draw the whole map, centred on screen, instead of a slice around the
    // hero. The map texture is drawn rotated 45 degrees, so a square of side
    // FULL_MAP_SIZE occupies FULL_MAP_SIZE * sqrt(2) on screen; keep it inside
    // the 430px-tall map area. The hero and markers are then plotted at their
    // real map positions rather than the map being scrolled under a fixed hero.
    const float FULL_MAP_SIZE = 285.f;
    const float MapCenterX = FULL_MAP_SIZE / 2.f;
    const float MapCenterY = FULL_MAP_SIZE / 2.f;

    float Ty1;
    float Tx1;
    float uvxy = (41.7f / 64.f);
    float uvxy_Line = 8.f / 8.f;
    float Ui_wid = 35.f;
    float Ui_Hig = 6.f;
    float Rot_Loc = 45.f;
    int i = 0;

    RenderBitRotate(IMAGE_MINIMAP_INTERFACE, MapCenterX, MapCenterY, FULL_MAP_SIZE, FULL_MAP_SIZE, Rot);

    int NpcWidth = 15;
    int NpcWidthP = 30;
    // MOBA arena: the static Crywolf markers (altars, NPCs) do not exist there.
    const bool mobaArena = gMapManager.WorldActive == WD_200_MOBA_ARENA;
    for (i = 0; i < MAX_MINI_MAP_DATA; i++)
    {
        if (m_Mini_Map_Data[i].Kind > 0 && !mobaArena)
        {
            Ty1 = (float)(((float)m_Mini_Map_Data[i].Location[0] / 256.f) * FULL_MAP_SIZE);
            Tx1 = (float)(((float)m_Mini_Map_Data[i].Location[1] / 256.f) * FULL_MAP_SIZE);
            Rot_Loc = (float)m_Mini_Map_Data[i].Rotation;

            if (m_Mini_Map_Data[i].Kind == 1) //npc
            {
                if (!(gMapManager.WorldActive == WD_34CRYWOLF_1ST && m_OccupationState > 0) || (m_Mini_Map_Data[i].Location[0] == 228 && m_Mini_Map_Data[i].Location[1] == 48 && gMapManager.WorldActive == WD_34CRYWOLF_1ST))
                    RenderPointRotate(IMAGE_MINIMAP_INTERFACE + 5, Tx1, Ty1, NpcWidth, NpcWidth, MapCenterX, MapCenterY, FULL_MAP_SIZE, FULL_MAP_SIZE, Rot, Rot_Loc, 17.5f / 32.f, 17.5f / 32.f, i);
            }
            else
                if (m_Mini_Map_Data[i].Kind == 2)
                    RenderPointRotate(IMAGE_MINIMAP_INTERFACE + 4, Tx1, Ty1, NpcWidthP, NpcWidthP, MapCenterX, MapCenterY, FULL_MAP_SIZE, FULL_MAP_SIZE, Rot, Rot_Loc, 17.5f / 32.f, 17.5f / 32.f, 100 + i);
        }
        else
            break;
    }

    if (g_MobaLevel > 0)
    {
        // Show the walk mask the client is using (edges of the blocked cells) so map edits can be checked in-game.
        glColor4f(0.85f, 0.25f, 0.25f, 1.f);
        const float cell = FULL_MAP_SIZE / 256.f * 1.3f;
        for (int cy = 1; cy < 255; ++cy)
        {
            for (int cx = 1; cx < 255; ++cx)
            {
                const int idx = cy * 256 + cx;
                if ((TerrainWall[idx] & TW_NOMOVE) != TW_NOMOVE)
                    continue;
                if ((TerrainWall[idx - 1] & TW_NOMOVE) && (TerrainWall[idx + 1] & TW_NOMOVE)
                    && (TerrainWall[idx - 256] & TW_NOMOVE) && (TerrainWall[idx + 256] & TW_NOMOVE))
                    continue;
                RenderPointRotateSquare(((float)cy / 256.f) * FULL_MAP_SIZE, ((float)cx / 256.f) * FULL_MAP_SIZE, cell,
                    MapCenterX, MapCenterY, FULL_MAP_SIZE, FULL_MAP_SIZE, Rot);
            }
        }
    }

    if (g_MobaLevel > 0)
    {
        for (int m = 0; m < g_MobaMinimapCount; ++m)
        {
            const MobaMinimapEntry& e = g_MobaMinimap[m];
            SetMobaMarkerColor(e);
            const float w = MobaMarkerSize(e, 12.f, 14.f, 20.f, 9.f);
            RenderPointRotateSquare(((float)e.y / 256.f) * FULL_MAP_SIZE, ((float)e.x / 256.f) * FULL_MAP_SIZE, w,
                MapCenterX, MapCenterY, FULL_MAP_SIZE, FULL_MAP_SIZE, Rot);
        }
        EndRenderColor();
        EnableAlphaTest();
    }

    float Ch_wid = 12;
    float HeroTx = ((float)Hero->PositionY / 256.f) * FULL_MAP_SIZE;
    float HeroTy = ((float)Hero->PositionX / 256.f) * FULL_MAP_SIZE;
    float HeroRot = Hero->Object.Angle[2];
    RenderPointRotate(IMAGE_MINIMAP_INTERFACE + 3, HeroTx, HeroTy, Ch_wid, Ch_wid, MapCenterX, MapCenterY, FULL_MAP_SIZE, FULL_MAP_SIZE, Rot, HeroRot, 17.5f / 32.f, 17.5f / 32.f, -1);

    for (i = 0; i < 25; i++)
    {
        RenderImage(IMAGE_MINIMAP_INTERFACE + 2, i * Ui_wid, 0, Ui_wid, Ui_Hig, 0.f, 1.f, uvxy, -uvxy_Line);
        RenderImage(IMAGE_MINIMAP_INTERFACE + 2, i * Ui_wid, 430 - Ui_Hig, Ui_wid, Ui_Hig, 0.f, 0.f, uvxy, uvxy_Line);
    }
    for (i = 0; i < 20; i++)
    {
        RenderBitmapRotate(IMAGE_MINIMAP_INTERFACE + 2, (Ui_Hig / 2.f), i * (Ui_wid - 3.f), Ui_wid, Ui_Hig, -90.f, 0.f, 0.f, uvxy, uvxy_Line);
        RenderBitmapRotate(IMAGE_MINIMAP_INTERFACE + 2, REFERENCE_WIDTH - (Ui_Hig / 2.f), i * (Ui_wid - 3.f), Ui_wid, Ui_Hig, 90.f, 0.f, 0.f, uvxy, uvxy_Line);
    }

    RenderImage(IMAGE_MINIMAP_INTERFACE + 1, 0, 0, Ui_wid, Ui_wid, 0.f, 0.f, uvxy, uvxy);
    RenderImage(IMAGE_MINIMAP_INTERFACE + 1, REFERENCE_WIDTH - Ui_wid, 0, Ui_wid, Ui_wid, uvxy, 0.f, -uvxy, uvxy);
    RenderImage(IMAGE_MINIMAP_INTERFACE + 1, 0, 430 - Ui_wid, Ui_wid, Ui_wid, 0.f, uvxy, uvxy, -uvxy);
    RenderImage(IMAGE_MINIMAP_INTERFACE + 1, REFERENCE_WIDTH - Ui_wid, 430 - Ui_wid, Ui_wid, Ui_wid, uvxy, uvxy, -uvxy, -uvxy);

    m_BtnExit.Render(true);

    DisableAlphaBlend();

    Check_Btn(MouseX, MouseY);
    return true;
}

void SEASON3B::CNewUIMiniMap::RenderCornerOverlayIfActive()
{
    if ((gMapManager.WorldActive == WD_200_MOBA_ARENA || gMapManager.WorldActive == WD_34CRYWOLF_1ST)
        && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MINI_MAP) == false)
    {
        RenderCornerMinimap();
    }
}

namespace
{
    // Corner-minimap layout, shared by the draw code and the mouse handling so
    // both always agree. All "ref" values are in the 640x480 reference space.
    struct CornerLayout
    {
        float sx, sy;       // reference -> screen px
        float size;         // width in ref px (the map is a true square on screen)
        float sizeY;        // height in ref px
        float sidePx;       // side of the square in screen px
        float cx, cy;       // map centre, ref px
        float opacity;      // 0.2 .. 1
        float markers;      // 0.5 .. 2
    };

    CornerLayout ComputeCornerLayout()
    {
        const GameConfig& cfg = GameConfig::GetInstance();
        CornerLayout l;
        l.sx = ConvertX(1.f);
        l.sy = ConvertY(1.f);
        // Scale 1..100 -> 32..210 ref px wide (50 = 120 ref px = 256 px at 1365 wide).
        l.size = 30.f + 1.8f * (float)cfg.GetMinimapScale();
        l.sizeY = l.size * l.sx / l.sy;
        l.sidePx = l.size * l.sx;
        l.opacity = cfg.GetMinimapOpacity() / 100.f;
        l.markers = cfg.GetMinimapMarkers() / 100.f;

        const float MARGIN = 0.f;     // flush with the screen edge
        const float HUD_BAR = 50.f;   // HUD bar height (its top edge is at y = 430 ref px)
        const float TOP_MARGIN = 8.f;
        const int corner = cfg.GetMinimapCorner();
        l.cx = (corner == 1) ? (MARGIN + l.size / 2.f) : (REFERENCE_WIDTH - l.size / 2.f - MARGIN);
        // Bottom corners clear the top of the HUD bar (no overlap).
        l.cy = (corner == 2) ? (TOP_MARGIN + l.sizeY / 2.f) : (REFERENCE_HEIGHT - HUD_BAR - l.sizeY / 2.f);
        return l;
    }

    // Options panel (reference px).
    constexpr int OPT_X = 240, OPT_Y = 120, OPT_W = 160, OPT_H = 156;
    constexpr int OPT_BAR_X = 10, OPT_BAR_W = 140;
    constexpr int OPT_ROW_Y[3] = { 24, 50, 76 };      // label rows: scale, opacity, markers
    constexpr int OPT_CORNER_Y = 104, OPT_CORNER_W = 44, OPT_BTN_H = 13;
    constexpr int OPT_BOTTOM_Y = 134;

    bool MouseIn(int x, int y, int w, int h)
    {
        return MouseX >= x && MouseX < x + w && MouseY >= y && MouseY < y + h;
    }

    void GearRect(const CornerLayout& l, int& x, int& y, int& w, int& h)
    {
        w = 13;
        h = (int)(13.f * l.sx / l.sy + 0.5f);
        // Flush with the screen border on the corner's side; on the map's lower
        // edge (just above the HUD bar) for the bottom corners, upper edge for the top one.
        const int corner = GameConfig::GetInstance().GetMinimapCorner();
        x = (corner == 1) ? 0 : (REFERENCE_WIDTH - w);
        y = (corner == 2) ? (int)(l.cy - l.sizeY / 2.f) : (int)(l.cy + l.sizeY / 2.f) - h;
    }

    // MOBA arena minimap: walkable ground keeps the original Crywolf art, walls
    // (the arena mask, plus every walkable pocket the players cannot reach from
    // the main ground: the islands the lanes go around) are drawn in one flat
    // style with a uniform 2-cell outline, and the solid rock outside the arena
    // stays transparent. Built on the CPU from the walk mask (TerrainWall) and
    // the art read back from the GPU. Texel (row = tile X, column = tile Y)
    // because the art's (u, v) are (tile Y, tile X). Rebuilt only when the walls
    // change (cheap hash checked on every frame).
    RHI::TextureHandle g_blockedTex{};
    unsigned int g_blockedHash = 0;

    bool UpdateBlockedTexture()
    {
        constexpr int N = 256;
        constexpr int TEX_SIZE = 1024;
        constexpr int SCALE = TEX_SIZE / N;
        unsigned int h = 2166136261u;
        for (int i = 0; i < N * N; ++i)
            h = (h ^ (unsigned int)(TerrainWall[i] & TW_NOMOVE)) * 16777619u;

        if (g_blockedTex.IsValid() && h == g_blockedHash)
            return true;

        static std::vector<unsigned char> blocked(N * N), exterior(N * N), cls(N * N);
        static std::vector<int> queue;
        for (int i = 0; i < N * N; ++i)
        {
            blocked[i] = (TerrainWall[i] & TW_NOMOVE) ? 1 : 0;
            exterior[i] = 0;
        }

        // Walkable cells the players cannot reach (everything but the largest
        // 4-connected walkable region) are islands: draw them as walls.
        {
            static std::vector<int> label(N * N);
            std::fill(label.begin(), label.end(), 0);
            int bestLabel = 0, bestSize = 0, nextLabel = 0;
            std::vector<int> stack;
            for (int start = 0; start < N * N; ++start)
            {
                if (blocked[start] || label[start])
                    continue;
                const int cur = ++nextLabel;
                int size = 0;
                stack.clear();
                stack.push_back(start);
                label[start] = cur;
                while (!stack.empty())
                {
                    const int i = stack.back();
                    stack.pop_back();
                    ++size;
                    const int x = i % N, y = i / N;
                    const int nb[4] = { x > 0 ? i - 1 : -1, x < N - 1 ? i + 1 : -1, y > 0 ? i - N : -1, y < N - 1 ? i + N : -1 };
                    for (int n : nb)
                        if (n >= 0 && !blocked[n] && !label[n]) { label[n] = cur; stack.push_back(n); }
                }
                if (size > bestSize) { bestSize = size; bestLabel = cur; }
            }
            for (int i = 0; i < N * N; ++i)
                if (!blocked[i] && label[i] != bestLabel)
                    blocked[i] = 1;
        }

        // Solid rock outside the arena: blocked cells reachable from the border through blocked cells.
        queue.clear();
        auto seed = [&](int x, int y)
        {
            const int i = y * N + x;
            if (blocked[i] && !exterior[i]) { exterior[i] = 1; queue.push_back(i); }
        };
        for (int k = 0; k < N; ++k) { seed(k, 0); seed(k, N - 1); seed(0, k); seed(N - 1, k); }
        for (size_t qi = 0; qi < queue.size(); ++qi)
        {
            const int x = queue[qi] % N, y = queue[qi] / N;
            if (x > 0) seed(x - 1, y);
            if (x < N - 1) seed(x + 1, y);
            if (y > 0) seed(x, y - 1);
            if (y < N - 1) seed(x, y + 1);
        }

        auto walkableNear = [&](int x, int y, int r)
        {
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx)
                {
                    const int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < N && ny >= 0 && ny < N && !blocked[ny * N + nx])
                        return true;
                }
            return false;
        };

        // Cell classes: 0 walkable (art), 1 wall outline, 2 wall interior, 3 outside rock (transparent).
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
            {
                const int i = y * N + x;
                if (!blocked[i]) cls[i] = 0;
                else if (walkableNear(x, y, 2)) cls[i] = 1;
                else cls[i] = exterior[i] ? 3 : 2;
            }

        // The original art, as the GPU holds it (same (u, v) orientation we sample with).
        static std::vector<unsigned int> art;
        int aw = 0, ah = 0;
        ReadBackBitmapRGBA(CNewUIMiniMap::IMAGE_MINIMAP_INTERFACE, art, aw, ah);

        // RGBA bytes as little-endian uint: 0xAABBGGRR.
        auto rgba = [](unsigned r, unsigned g, unsigned b, unsigned a) { return (a << 24) | (b << 16) | (g << 8) | r; };
        const unsigned int cWalk   = rgba(178, 150, 100, 255);  // flat fallback under / without the art
        const unsigned int cWall   = rgba(24, 18, 12, 255);     // outline, 2 cells
        const unsigned int cIsland = rgba(70, 56, 38, 255);     // wall interior

        static std::vector<unsigned int> px((size_t)TEX_SIZE * TEX_SIZE);
        for (int r = 0; r < TEX_SIZE; ++r)
        {
            const int tx = r / SCALE;
            for (int c = 0; c < TEX_SIZE; ++c)
            {
                const int ty = c / SCALE;
                const unsigned char k = cls[ty * N + tx];
                unsigned int out;
                if (k == 0)
                {
                    out = cWalk;
                    if (aw > 0)
                    {
                        const unsigned int a = art[(size_t)(r * ah / TEX_SIZE) * aw + (c * aw / TEX_SIZE)];
                        const unsigned int al = a >> 24;
                        const unsigned int rr = ((a & 255) * al + (cWalk & 255) * (255 - al)) / 255;
                        const unsigned int gg = (((a >> 8) & 255) * al + ((cWalk >> 8) & 255) * (255 - al)) / 255;
                        const unsigned int bb = (((a >> 16) & 255) * al + ((cWalk >> 16) & 255) * (255 - al)) / 255;
                        out = rgba(rr, gg, bb, 255);
                    }
                }
                else if (k == 1) out = cWall;
                else if (k == 2) out = cIsland;
                else out = 0u;
                px[(size_t)r * TEX_SIZE + c] = out;
            }
        }

        if (!g_blockedTex.IsValid())
        {
            RHI::TextureDesc desc{ TEX_SIZE, TEX_SIZE, RHI::TexFilter::Linear, RHI::TexWrap::Clamp };
            g_blockedTex = RHI::CreateTexture(desc, px.data());
        }
        else
        {
            RHI::UpdateTexture(g_blockedTex, 0, 0, TEX_SIZE, TEX_SIZE, px.data());
        }
        g_blockedHash = h;
        return g_blockedTex.IsValid();
    }

    bool CornerMinimapActive(bool success)
    {
        return success && Hero != NULL
            && (gMapManager.WorldActive == WD_200_MOBA_ARENA || gMapManager.WorldActive == WD_34CRYWOLF_1ST)
            && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MINI_MAP) == false;
    }
}

void SEASON3B::CNewUIMiniMap::RenderCornerMinimap()
{
    if (m_bSuccess == false || Hero == NULL)
        return;

    // Small map pinned to a screen corner. Drawn with RenderBitmapRotate (same
    // family as the full-map RenderBitRotate) so the texture's transparent
    // corners are honoured. The window is not 4:3, so a square in reference px
    // is a rectangle on screen, and RenderBitmapRotate rotates in screen px
    // (after scaling): the art came out squashed while the markers assumed a
    // square. Draw the map as a true square on screen (size ref px wide,
    // size * sx/sy tall) and place markers in screen px, converting back to
    // reference px.
    const CornerLayout L = ComputeCornerLayout();
    const float sx = L.sx;
    const float sy = L.sy;
    const float SIZE = L.size;
    const float SIZE_Y = L.sizeY;
    const float SIDE_PX = L.sidePx;
    const float CENTRE_X = L.cx;
    const float CENTRE_Y = L.cy;

    // Whole-texture rotation. 90 = one quarter turn; flip the sign to spin the
    // other way.
    const float ROT = 90.f;
    const float UV = 17.5f / 32.f;

    const float rad = ROT * 3.14159265f / 180.f;
    const float ca = cosf(rad);
    const float sa = sinf(rad);

    // Map art: blended so the opacity option works; transparent corners stay so.
    EnableAlphaBlend();
    EnableAlphaTest();
    glColor4f(1.f, 1.f, 1.f, L.opacity);

    // MOBA arena: our own map from the walk mask; elsewhere (Crywolf) the original art.
    if (gMapManager.WorldActive == WD_200_MOBA_ARENA && UpdateBlockedTexture())
        RenderBitmapRotate(-(int)g_blockedTex.id, CENTRE_X, CENTRE_Y, SIZE, SIZE_Y, ROT, 0.f, 0.f, 1.f, 1.f);
    else
        RenderBitmapRotate(IMAGE_MINIMAP_INTERFACE, CENTRE_X, CENTRE_Y, SIZE, SIZE_Y, ROT, 0.f, 0.f, 1.f, 1.f);

    DisableAlphaBlend();
    EnableAlphaTest();
    glColor4f(1.f, 1.f, 1.f, 1.f);

    // Map-local position (0..1, texture space) to reference px: rotate the
    // offset by ROT around the map centre in screen px, matching how
    // RenderBitmapRotate rotated the texture.
    auto toRef = [&](float pu, float pv, float& mx, float& my)
    {
        const float lx = (pu - 0.5f) * SIDE_PX;
        const float ly = (0.5f - pv) * SIDE_PX;
        mx = CENTRE_X + (lx * ca - ly * sa) / sx;
        my = CENTRE_Y - (lx * sa + ly * ca) / sy;
    };

    // Plot a marker, then blit the icon (square on screen).
    auto plot = [&](int tex, float pu, float pv, float w, float rotLoc)
    {
        float mx, my;
        toRef(pu, pv, mx, my);
        w *= L.markers;
        RenderBitmapRotate(tex, mx, my, w, w * sx / sy, rotLoc, 0.f, 0.f, UV, UV);
    };

    for (int i = 0; i < MAX_MINI_MAP_DATA; i++)
    {
        if (m_Mini_Map_Data[i].Kind <= 0 || gMapManager.WorldActive == WD_200_MOBA_ARENA)
            break;

        const float pu = (float)m_Mini_Map_Data[i].Location[1] / 256.f;
        const float pv = (float)m_Mini_Map_Data[i].Location[0] / 256.f;

        if (m_Mini_Map_Data[i].Kind == 1)
            plot(IMAGE_MINIMAP_INTERFACE + 5, pu, pv, 7.f, 0.f);
        else if (m_Mini_Map_Data[i].Kind == 2)
            plot(IMAGE_MINIMAP_INTERFACE + 4, pu, pv, 12.f, 0.f);
    }

    if (g_MobaLevel > 0)
    {
        for (int m = 0; m < g_MobaMinimapCount; ++m)
        {
            const MobaMinimapEntry& e = g_MobaMinimap[m];
            SetMobaMarkerColor(e);
            const float w = MobaMarkerSize(e, 6.f, 7.f, 11.f, 5.f) * L.markers;
            float mx, my;
            toRef((float)e.y / 256.f, (float)e.x / 256.f, mx, my);
            const float h = w * sx / sy;
            RenderColor(mx - w / 2.f, my - h / 2.f, w, h);
        }
        EndRenderColor();
        EnableAlphaTest();
    }

    plot(IMAGE_MINIMAP_INTERFACE + 3, (float)Hero->PositionY / 256.f, (float)Hero->PositionX / 256.f, 8.f, 0.f);

    DisableAlphaBlend();

    // Options gear (opens the panel) and the panel itself.
    int gx, gy, gw, gh;
    GearRect(L, gx, gy, gw, gh);
    const bool gearHot = MouseIn(gx, gy, gw, gh);
    glColor4f(0.f, 0.f, 0.f, gearHot || m_bOptionsOpen ? 0.85f : 0.55f);
    RenderColor((float)gx, (float)gy, (float)gw, (float)gh, 0.f);
    glColor4f(1.f, 1.f, 1.f, gearHot || m_bOptionsOpen ? 1.f : 0.75f);
    for (int k = 0; k < 3; ++k)
        RenderColor((float)gx + 2.f, (float)gy + 2.f + k * (gh - 5) / 2.f, (float)gw - 4.f, 1.f, 0.f);
    EndRenderColor();

    if (m_bOptionsOpen)
        RenderCornerOptions();
}

void SEASON3B::CNewUIMiniMap::RenderCornerOptions()
{
    GameConfig& cfg = GameConfig::GetInstance();

    glColor4f(0.f, 0.f, 0.f, 0.88f);
    RenderColor((float)OPT_X, (float)OPT_Y, (float)OPT_W, (float)OPT_H, 0.f);
    glColor4f(0.8f, 0.7f, 0.3f, 1.f);
    RenderColor((float)OPT_X, (float)OPT_Y, (float)OPT_W, 1.f, 0.f);
    RenderColor((float)OPT_X, (float)(OPT_Y + OPT_H - 1), (float)OPT_W, 1.f, 0.f);
    RenderColor((float)OPT_X, (float)OPT_Y, 1.f, (float)OPT_H, 0.f);
    RenderColor((float)(OPT_X + OPT_W - 1), (float)OPT_Y, 1.f, (float)OPT_H, 0.f);

    struct Row { const wchar_t* label; int value; int minV, maxV; const wchar_t* unit; };
    const Row rows[3] = {
        { L"Tamaño",     cfg.GetMinimapScale(),   1,  100, L"" },
        { L"Opacidad",        cfg.GetMinimapOpacity(), 20, 100, L"%" },
        { L"Marcadores",      cfg.GetMinimapMarkers(), 50, 200, L"%" },
    };
    for (int i = 0; i < 3; ++i)
    {
        const int by = OPT_Y + OPT_ROW_Y[i] + 12;
        glColor4f(0.25f, 0.25f, 0.25f, 1.f);
        RenderColor((float)(OPT_X + OPT_BAR_X), (float)(by + 2), (float)OPT_BAR_W, 3.f, 0.f);
        const float t = (float)(rows[i].value - rows[i].minV) / (float)(rows[i].maxV - rows[i].minV);
        glColor4f(0.9f, 0.8f, 0.3f, 1.f);
        RenderColor((float)(OPT_X + OPT_BAR_X) + t * (OPT_BAR_W - 4), (float)by, 4.f, 7.f, 0.f);
    }

    // Position + action buttons.
    const wchar_t* cornerLabels[3] = { L"Inf. der.", L"Inf. izq.", L"Sup. der." };
    for (int c = 0; c < 3; ++c)
    {
        const bool sel = cfg.GetMinimapCorner() == c;
        glColor4f(sel ? 0.55f : 0.2f, sel ? 0.45f : 0.2f, sel ? 0.1f : 0.2f, 1.f);
        RenderColor((float)(OPT_X + OPT_BAR_X + c * (OPT_CORNER_W + 4)), (float)(OPT_Y + OPT_CORNER_Y + 12), (float)OPT_CORNER_W, (float)OPT_BTN_H, 0.f);
    }
    glColor4f(0.2f, 0.2f, 0.2f, 1.f);
    RenderColor((float)(OPT_X + 10), (float)(OPT_Y + OPT_BOTTOM_Y), 68.f, (float)OPT_BTN_H, 0.f);
    RenderColor((float)(OPT_X + 82), (float)(OPT_Y + OPT_BOTTOM_Y), 68.f, (float)OPT_BTN_H, 0.f);
    EndRenderColor();

    DWORD backColor = g_pRenderText->GetTextColor();
    DWORD backBg = g_pRenderText->GetBgColor();
    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(255, 230, 150, 255);
    g_pRenderText->RenderText(OPT_X + 10, OPT_Y + 7, L"Minimapa", OPT_W - 20, 0, RT3_SORT_LEFT);
    g_pRenderText->SetTextColor(255, 255, 255, 255);
    for (int i = 0; i < 3; ++i)
    {
        wchar_t buf[48];
        swprintf(buf, 48, L"%ls: %d%ls", rows[i].label, rows[i].value, rows[i].unit);
        g_pRenderText->RenderText(OPT_X + 10, OPT_Y + OPT_ROW_Y[i], buf, OPT_W - 20, 0, RT3_SORT_LEFT);
    }
    g_pRenderText->RenderText(OPT_X + 10, OPT_Y + OPT_CORNER_Y, L"Posición", OPT_W - 20, 0, RT3_SORT_LEFT);
    for (int c = 0; c < 3; ++c)
        g_pRenderText->RenderText(OPT_X + OPT_BAR_X + c * (OPT_CORNER_W + 4), OPT_Y + OPT_CORNER_Y + 13, cornerLabels[c], OPT_CORNER_W, 0, RT3_SORT_CENTER);
    g_pRenderText->RenderText(OPT_X + 10, OPT_Y + OPT_BOTTOM_Y + 1, L"Restaurar", 68, 0, RT3_SORT_CENTER);
    g_pRenderText->RenderText(OPT_X + 82, OPT_Y + OPT_BOTTOM_Y + 1, L"Cerrar", 68, 0, RT3_SORT_CENTER);
    g_pRenderText->SetTextColor(backColor);
    g_pRenderText->SetBgColor(backBg);
}

// Mouse handling for the corner minimap's options gear and panel. Called by
// the UI manager before any window sees the mouse; returns true when the
// click was used here (so it does not also walk / attack in the world).
bool SEASON3B::CNewUIMiniMap::UpdateCornerMouse()
{
    if (!CornerMinimapActive(m_bSuccess))
    {
        m_bOptionsOpen = false;
        m_iDragSlider = -1;
        m_bSwallowHold = false;
        return false;
    }

    GameConfig& cfg = GameConfig::GetInstance();
    const bool press = SEASON3B::IsPress(VK_LBUTTON);
    const bool held = press || SEASON3B::IsRepeat(VK_LBUTTON);

    auto setSlider = [&](int row, int mouseX)
    {
        float t = (float)(mouseX - (OPT_X + OPT_BAR_X)) / (float)OPT_BAR_W;
        t = std::clamp(t, 0.f, 1.f);
        if (row == 0) cfg.SetMinimapScale(1 + (int)(t * 99.f + 0.5f));
        else if (row == 1) cfg.SetMinimapOpacity(20 + (int)(t * 80.f + 0.5f));
        else cfg.SetMinimapMarkers(50 + (int)(t * 150.f + 0.5f));
    };

    if (m_iDragSlider >= 0)
    {
        if (held)
        {
            setSlider(m_iDragSlider, MouseX);
            return true;
        }
        m_iDragSlider = -1;
        cfg.Save();
        return true;
    }

    // A click used here (e.g. Cerrar, which hides the panel under the cursor)
    // must not reach the world while the same button press is still held.
    if (m_bSwallowHold)
    {
        if (held)
            return true;
        m_bSwallowHold = false;
    }

    const CornerLayout L = ComputeCornerLayout();
    int gx, gy, gw, gh;
    GearRect(L, gx, gy, gw, gh);
    if (MouseIn(gx, gy, gw, gh))
    {
        if (press)
        {
            m_bOptionsOpen = !m_bOptionsOpen;
            m_bSwallowHold = true;
            PlayBuffer(SOUND_CLICK01);
        }
        return true;
    }

    if (!m_bOptionsOpen || !MouseIn(OPT_X, OPT_Y, OPT_W, OPT_H))
        return false;

    if (!press)
        return true;

    m_bSwallowHold = true;
    for (int i = 0; i < 3; ++i)
    {
        if (MouseIn(OPT_X + OPT_BAR_X - 4, OPT_Y + OPT_ROW_Y[i] + 8, OPT_BAR_W + 8, 15))
        {
            m_iDragSlider = i;
            setSlider(i, MouseX);
            return true;
        }
    }
    for (int c = 0; c < 3; ++c)
    {
        if (MouseIn(OPT_X + OPT_BAR_X + c * (OPT_CORNER_W + 4), OPT_Y + OPT_CORNER_Y + 12, OPT_CORNER_W, OPT_BTN_H))
        {
            cfg.SetMinimapCorner(c);
            cfg.Save();
            return true;
        }
    }
    if (MouseIn(OPT_X + 10, OPT_Y + OPT_BOTTOM_Y, 68, OPT_BTN_H))
    {
        cfg.ResetMinimap();
        cfg.Save();
    }
    else if (MouseIn(OPT_X + 82, OPT_Y + OPT_BOTTOM_Y, 68, OPT_BTN_H))
    {
        m_bOptionsOpen = false;
    }
    return true;
}

bool SEASON3B::CNewUIMiniMap::Update()
{
    return true;
}

void SEASON3B::CNewUIMiniMap::LoadImages(const wchar_t* Filename)
{
    wchar_t Fname[300];
    int i = 0;
    mu_swprintf(Fname, L"Data\\%ls\\mini_map.ozt", Filename);
    FILE* pFile = _wfopen(Fname, L"rb");

    if (pFile == NULL)
    {
        m_bSuccess = false;
        return;
    }
    else
    {
        m_bSuccess = true;
        fclose(pFile);
        mu_swprintf(Fname, L"%ls\\mini_map.tga", Filename);
        LoadBitmap(Fname, IMAGE_MINIMAP_INTERFACE, GL_LINEAR);
    }

    mu_swprintf(Fname, L"Data\\Local\\%ls\\Minimap\\Minimap_%ls_%ls.bmd", g_strSelectedML.c_str(), Filename, g_strSelectedML.c_str());

    for (i = 0; i < MAX_MINI_MAP_DATA; i++)
    {
        m_Mini_Map_Data[i].Kind = 0;
    }

    FILE* fp = _wfopen(Fname, L"rb");

    if (fp != NULL)
    {
        int Size = sizeof(MINI_MAP_FILE);
        BYTE* Buffer = new BYTE[Size * MAX_MINI_MAP_DATA + 45];
        fread(Buffer, (Size * MAX_MINI_MAP_DATA) + 45, 1, fp);

        DWORD dwCheckSum;
        fread(&dwCheckSum, sizeof(DWORD), 1, fp);
        fclose(fp);

        if (dwCheckSum != GenerateCheckSum2(Buffer, (Size * MAX_MINI_MAP_DATA) + 45, 0x2BC1))
        {
            wchar_t Text[256];
            mu_swprintf(Text, L"%ls - File corrupted.", Fname);
            g_ErrorReport.Write(Text);
            MessageBox(g_hWnd, Text, NULL, MB_OK);
            SendMessage(g_hWnd, WM_DESTROY, 0, 0);
        }
        else
        {
            BYTE* pSeek = Buffer;

            for (i = 0; i < MAX_MINI_MAP_DATA; i++)
            {
                BuxConvert(pSeek, Size);
                //memcpy(&(m_Mini_Map_Data[i]), pSeek, Size);

                MINI_MAP_FILE current{ };
                auto target = &(m_Mini_Map_Data[i]);
                memcpy(&current, pSeek, Size);
                memcpy(target, pSeek, Size);

                CMultiLanguage::ConvertFromUtf8(target->Name, current.Name);
                /*int wchars_num = MultiByteToWideChar(CP_UTF8, 0, current.Name, -1, NULL, 0);
                MultiByteToWideChar(CP_UTF8, 0, current.Name, -1, target->Name, wchars_num);
                target->Name[wchars_num] = L'\0';*/
                pSeek += Size;
            }
        }

        delete[] Buffer;
    }
}

void SEASON3B::CNewUIMiniMap::UnloadImages()
{
    DeleteBitmap(IMAGE_MINIMAP_INTERFACE);
}

bool SEASON3B::CNewUIMiniMap::UpdateMouseEvent()
{
    bool ret = true;

    if (m_BtnExit.UpdateMouseEvent() == true)
    {
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_MINI_MAP);
        return true;
    }

    if (IsPress(VK_LBUTTON))
    {
        ret = Check_Mouse(MouseX, MouseY);
        if (ret == false)
        {
            PlayBuffer(SOUND_CLICK01);
        }
    }

    if (CheckMouseIn(0, 0, REFERENCE_WIDTH, 430))
    {
        return false;
    }

    return ret;
}

bool SEASON3B::CNewUIMiniMap::Check_Mouse(int mx, int my)
{
    return true;
}

bool SEASON3B::CNewUIMiniMap::Check_Btn(int mx, int my)
{
    int i = 0;
    for (i = 0; i < MAX_MINI_MAP_DATA; i++)
    {
        if (m_Mini_Map_Data[i].Kind > 0)
        {
            if (mx > m_Btn_Loc[i][0] && mx < (m_Btn_Loc[i][0] + m_Btn_Loc[i][2]) && my > m_Btn_Loc[i][1] && my < (m_Btn_Loc[i][1] + m_Btn_Loc[i][3]))
            {
                SIZE Fontsize;
                m_TooltipText = (std::wstring)m_Mini_Map_Data[i].Name;
                g_pRenderText->SetFont(g_hFont);
                GetTextExtentPoint32(g_pRenderText->GetFontDC(), m_TooltipText.c_str(), m_TooltipText.size(), &Fontsize);

                Fontsize.cx = Fontsize.cx / ((float)WindowWidth / REFERENCE_WIDTH);
                Fontsize.cy = Fontsize.cy / ((float)WindowHeight / REFERENCE_HEIGHT);

                int x = m_Btn_Loc[i][0] + ((m_Btn_Loc[i][2] / 2) - (Fontsize.cx / 2));
                int y = m_Btn_Loc[i][1] + m_Btn_Loc[i][3] + 2;

                y = m_Btn_Loc[i][1] - (Fontsize.cy + 2);

                DWORD backuptextcolor = g_pRenderText->GetTextColor();
                DWORD backuptextbackcolor = g_pRenderText->GetBgColor();

                g_pRenderText->SetTextColor(RGBA(255, 255, 255, 255));
                g_pRenderText->SetBgColor(RGBA(0, 0, 0, 180));
                g_pRenderText->RenderText(x, y, m_TooltipText.c_str(), Fontsize.cx + 6, 0, RT3_SORT_CENTER);

                g_pRenderText->SetTextColor(backuptextcolor);
                g_pRenderText->SetBgColor(backuptextbackcolor);

                return true;
            }
        }
        else
            break;
    }
    return false;
}