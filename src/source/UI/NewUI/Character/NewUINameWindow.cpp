// NewUINameWindow.cpp: implementation of the CNewUINameWindow class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "UI/Chat/Chat.h"
#include "UI/NewUI/Character/NewUINameWindow.h"
#include "Render/Models/ZzzBMD.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzInventory.h"
#include "UI/Legacy/UIControls.h"
#include "GameLogic/Events/CSChaosCastle.h"
#include "GameLogic/Items/PersonalShopTitleImp.h"
#include "GameLogic/Events/MatchEvent.h"
#include "World/Moba/MobaFog.h"
#include "World/MapInfra/MapManager.h"
#include "Camera/CameraProjection.h"
#include "Camera/CameraState.h"

// DevEditor forward declarations (must be at global scope)
#ifdef _EDITOR
extern "C" bool DevEditor_ShouldRenderItemLabels();
#endif

using namespace SEASON3B;

namespace
{
constexpr int GROUND_ITEM_LABEL_BUILD_BUDGET_PER_FRAME = 32;

// Draws a segmented monster HP bar, horizontally centered on centerX with its
// top edge at topY. `steps` is the segment count (HP granularity); `scale`
// horizontally compresses the bar (1.0 == original width).
void DrawHealthBar(int centerX, int topY, float health, int steps, float scale,
                   float fillR = 250.f / 255.f, float fillG = 10.f / 255.f, float fillB = 0.f, float thickness = 1.f)
{
    const float borderHeight = 2.f * thickness;      // vertical inset (unscaled)
    const float borderWidth = 2.f * scale;           // horizontal inset
    const float stepSeparatorWidth = 1.f * scale;    // gap between segments
    const float segmentSpan = 80.f * scale;          // total span of the segment track
    const float widthPerStep = segmentSpan / steps;  // derived: fewer steps -> wider segments
    const float stepsWidth = segmentSpan - 2.f * stepSeparatorWidth;
    const float totalWidth = stepsWidth + borderWidth * 2.f;

    const int x = centerX - (int)(totalWidth / 2);
    const int y = topY;

    // Drop shadow.
    EnableAlphaTest();
    glColor4f(0.f, 0.f, 0.f, 0.5f);
    RenderColor((float)(x + 1), (float)(y + 1), totalWidth, 5.f * thickness);

    // Dark backing + inner track, derived from the fill colour so the empty part of a
    // (e.g. blue) team bar doesn't show through as the vanilla dark red.
    EnableAlphaBlend();
    glColor3f(fillR * 0.22f, fillG * 0.22f, fillB * 0.22f);
    RenderColor((float)x, (float)y, totalWidth, 5.f * thickness);

    glColor3f(fillR * 0.35f, fillG * 0.35f, fillB * 0.35f);
    RenderColor((float)(x + borderWidth), (float)(y + borderHeight), stepsWidth, 1.f * thickness);

    // HealthStatus < 0 is the "HP unknown" sentinel (server sends 0xFF -> -1, and
    // the field is initialized to -1), so render a full bar instead of an empty one.
    const float clampedHealth = (health < 0.f) ? 1.f : health;
    const int stepHP = (int)(clampedHealth * steps);

    // Filled health segments.
    glColor3f(fillR, fillG, fillB);
    for (int k = 0; k < stepHP; ++k)
    {
        RenderColor(
            (float)(x + borderWidth + (k * widthPerStep)),
            (float)(y + borderHeight),
            widthPerStep - stepSeparatorWidth,
            2.f * thickness);
    }
    DisableAlphaBlend();
}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SEASON3B::CNewUINameWindow::CNewUINameWindow()
{
    m_pNewUIMng = NULL;
    m_Pos.x = m_Pos.y = 0;

    m_bShowItemName = false;
    m_bShowMonsterHealthBar = false;
}

SEASON3B::CNewUINameWindow::~CNewUINameWindow()
{
    Release();
}

bool SEASON3B::CNewUINameWindow::Create(CNewUIManager* pNewUIMng, int x, int y)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_NAME_WINDOW, this);

    SetPos(x, y);

    Show(true);

    return true;
}

void SEASON3B::CNewUINameWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }
}

void SEASON3B::CNewUINameWindow::SetPos(int x, int y)
{
    m_Pos.x = x;
    m_Pos.y = y;
}

bool SEASON3B::CNewUINameWindow::UpdateMouseEvent()
{
    return true;
}

bool SEASON3B::CNewUINameWindow::UpdateKeyEvent()
{
    if (SEASON3B::IsPress(VK_MENU) == true)
    {
        m_bShowItemName = !m_bShowItemName;
    }

    if (SEASON3B::IsPress(VK_F8) == true)
    {
        m_bShowMonsterHealthBar = !m_bShowMonsterHealthBar;
    }

    return true;
}

bool SEASON3B::CNewUINameWindow::Update()
{
    return true;
}

bool SEASON3B::CNewUINameWindow::Render()
{
    EnableAlphaTest();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    RenderName();
    RenderTimes();
    matchEvent::RenderMatchTimes();
    UI::Chat::RenderBooleans();
    RenderMonsterHealthBars();
    RenderMobaHealthBars();
    DrawPersonalShopTitleImp();
    DisableAlphaBlend();
    return true;
}

void SEASON3B::CNewUINameWindow::RenderName()
{
    if (g_bGMObservation == true)
    {
        for (int i = 0; i < MAX_CHARACTERS_CLIENT; i++)
        {
            CHARACTER* c = &CharactersClient[i];
            OBJECT* o = &c->Object;
            if (o->Live && o->Kind == KIND_PLAYER)
            {
                if (IsShopTitleVisible(c) == false)
                {
                    UI::Chat::CreateChat(c->ID, L"", c);
                }
            }
        }
    }

#ifndef GUILD_WAR_EVENT
    if (gMapManager.InChaosCastle() == true && (SelectedNpc != -1 || SelectedCharacter != -1))
    {
        return;
    }
#endif//GUILD_WAR_EVENT

    if (SelectedItem != -1 || SelectedNpc != -1 || SelectedCharacter != -1)
    {
        if (SelectedNpc != -1)
        {
            CHARACTER* c = &CharactersClient[SelectedNpc];
            OBJECT* o = &c->Object;
            UI::Chat::CreateChat(c->ID, L"", c);
        }
        else if (SelectedCharacter != -1)
        {
            CHARACTER* c = &CharactersClient[SelectedCharacter];

            OBJECT* o = &c->Object;
            if (o->Kind == KIND_MONSTER)
            {
                g_pRenderText->SetTextColor(255, 230, 200, 255);
                g_pRenderText->SetBgColor(100, 0, 0, 255);
                g_pRenderText->RenderText(320, 2, c->ID, 0, 0, RT3_WRITE_CENTER);

                if (c->HealthStatus > 0)
                {
                    // Full-width bar centered under the selected monster's name.
                    DrawHealthBar(320, 15, c->HealthStatus, 20, 1.f);
                }
            }
            else
#ifdef ASG_ADD_GENS_SYSTEM
#ifndef PBG_MOD_STRIFE_GENSMARKRENDER
                if (!::IsStrifeMap(World) || Hero->m_byGensInfluence == c->m_byGensInfluence)
#endif //PBG_MOD_STRIFE_GENSMARKRENDER
#endif	// ASG_ADD_GENS_SYSTEM
                {
                    if (IsShopTitleVisible(c) == false)
                    {
                        UI::Chat::CreateChat(c->ID, L"", c);
                    }
                }
        }
        else if (SelectedItem != -1)
        {
#ifdef _EDITOR
            if (DevEditor_ShouldRenderItemLabels())
#endif
                RenderItemName(SelectedItem, &Items[SelectedItem].Object, &Items[SelectedItem].Item, false);
        }
    }

    if (m_bShowItemName || SEASON3B::IsRepeat(VK_MENU))
    {
        SetGroundItemLabelBuildBudget(GROUND_ITEM_LABEL_BUILD_BUDGET_PER_FRAME);

#ifdef _EDITOR
        bool renderLabels = DevEditor_ShouldRenderItemLabels();
#else
        bool renderLabels = true;
#endif

        if (renderLabels)
        {
            for (int i = 0; i < MAX_ITEMS; i++)
            {
                OBJECT* o = &Items[i].Object;
                if (o->Live)
                {
                    if (o->Visible && i != SelectedItem)
                    {
                        RenderItemName(i, o, &Items[i].Item, true);
                    }
                }
            }
        }
    }
}

void SEASON3B::CNewUINameWindow::RenderMonsterHealthBars()
{
    if (!m_bShowMonsterHealthBar)
        return;

    for (int i = 0; i < MAX_CHARACTERS_CLIENT; i++)
    {
        CHARACTER* c = &CharactersClient[i];
        OBJECT* o = &c->Object;

        if (!o->Live || !o->Visible || o->Alpha <= 0.f || c->Dead > 0 || o->Kind != KIND_MONSTER)
            continue;

        vec3_t Position;
        Vector(o->Position[0], o->Position[1], o->Position[2] + o->BoundingBoxMax[2] + 60.f, Position);

        int ScreenX, ScreenY;
        vec3_t transformPos;
        VectorTransform(Position, g_Camera.Matrix, transformPos);
        if (transformPos[2] >= 0)
            continue;

        CameraProjection::WorldToScreen(g_Camera, Position, &ScreenX, &ScreenY);

        if (ScreenX < -100 || ScreenY < -100
            || ScreenX > (REFERENCE_WIDTH + 100)
            || ScreenY > (REFERENCE_HEIGHT + 100))
            continue;

        // Bar fixed at ~3/7 of the original width, with 8 segments so each one
        // stays close to the original thickness (see DrawHealthBar for geometry).
        DrawHealthBar(ScreenX, ScreenY, c->HealthStatus, 8, 3.f / 7.f);
    }
}

void SEASON3B::CNewUINameWindow::RenderMobaHealthBars()
{
    // Diagnostics (every 10 s in the MuError.log): how many characters are alive, have a team, a fresh status and get a bar.
    static double s_nextReport = 0.0;
    int liveCount = 0, teamedCount = 0, freshCount = 0, drawnCount = 0;

    for (int i = 0; i < MAX_CHARACTERS_CLIENT; i++)
    {
        CHARACTER* c = &CharactersClient[i];
        OBJECT* o = &c->Object;

        if (o->Live && (o->Kind == KIND_MONSTER || o->Kind == KIND_PLAYER))
        {
            ++liveCount;
            if (c->MobaTeam != 0)
                ++teamedCount;
            if (c->MobaTeam != 0 && (WorldTime - c->MobaStatusTime) <= 3500.0)
                ++freshCount;
        }

        // Any MOBA participant with a fresh team-status update - creeps AND champion
        // players, the own hero included (2026-10-17: the user wants to see its own bar over the character too); the F8 toggle is ignored here.
        if (c->MobaTeam == 0 || (WorldTime - c->MobaStatusTime) > 3500.0)
            continue;
        if (!o->Live || !o->Visible || o->Alpha <= 0.f || c->Dead > 0)
            continue;
        if (o->Kind != KIND_MONSTER && o->Kind != KIND_PLAYER)
            continue;

        // What the fog hides is not drawn, and neither is its bar.
        if (c != Hero && World::Moba::Fog::HidesCharacter(c->MobaTeam, (int)(o->Position[0] / TERRAIN_SCALE), (int)(o->Position[1] / TERRAIN_SCALE)))
            continue;

        vec3_t Position;
        Vector(o->Position[0], o->Position[1], o->Position[2] + o->BoundingBoxMax[2] + 60.f, Position);

        vec3_t transformPos;
        VectorTransform(Position, g_Camera.Matrix, transformPos);
        if (transformPos[2] >= 0)
            continue;

        int ScreenX, ScreenY;
        CameraProjection::WorldToScreen(g_Camera, Position, &ScreenX, &ScreenY);
        if (ScreenX < -100 || ScreenY < -100
            || ScreenX > (REFERENCE_WIDTH + 100)
            || ScreenY > (REFERENCE_HEIGHT + 100))
            continue;

        const bool blue = (c->MobaTeam == 1);
        const bool neutral = (c->MobaTeam == 3);   // jungle monsters
        // 2026-10-17: longer and thicker bars (the user found them too small). Champions get the biggest, so they read as the important targets.
        const bool champion = (o->Kind == KIND_PLAYER);
        const int segments = champion ? 24 : 12;
        const float length = champion ? 0.95f : 0.60f;
        const float thickness = champion ? 1.8f : 1.4f;
        const float barHeight = (champion ? 5.f : 4.f) * thickness;
        DrawHealthBar(ScreenX, ScreenY - (int)barHeight, c->MobaHealth, segments, length,
                      neutral ? 1.00f : (blue ? 0.15f : 1.00f),
                      neutral ? 0.75f : (blue ? 0.35f : 0.15f),
                      neutral ? 0.10f : (blue ? 1.00f : 0.15f),
                      thickness);
        ++drawnCount;
    }

    if (WorldTime >= s_nextReport)
    {
        s_nextReport = WorldTime + 10000.0;
        if (gMapManager.WorldActive == WD_200_MOBA_ARENA)
            g_ErrorReport.Write(L"[MOBA-BARS] live=%d teamed=%d fresh=%d drawn=%d\r\n", liveCount, teamedCount, freshCount, drawnCount);
    }
}

float SEASON3B::CNewUINameWindow::GetLayerDepth()
{
    return 1.0f;
}
