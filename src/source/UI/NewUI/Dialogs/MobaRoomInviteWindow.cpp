#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaRoomInviteWindow.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUICommon.h"             // IsPress
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "Render/Textures/ZzzOpenglUtil.h"    // EndRenderColor
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaRoom.h"

#include <cmath>

using namespace SEASON3B;
namespace Room = World::Moba::Room;
namespace Draw = UI::Moba::Draw;
using Draw::Rect;

namespace
{
    // Layout, in the 640x480 reference screen.
    constexpr int PANEL_W = 360;
    constexpr int PANEL_H = 118;
    constexpr int PANEL_X = (REFERENCE_WIDTH - PANEL_W) / 2;
    constexpr int PANEL_Y = 60;

    constexpr int TITLE_Y = 8;
    constexpr int FROM_Y = 40;
    constexpr int ROOM_Y = 58;
    constexpr int BUTTONS_Y = 84;
    constexpr int BUTTON_W = 120;
    constexpr int BUTTON_H = 26;
    constexpr int BUTTON_GAP = 20;

    constexpr float LAYER_DEPTH = 10.75f;       // above the room panel and browser, below the ready check
    constexpr float KEY_EVENT_ORDER = 95.0f;
    constexpr DWORD UI_KEY_OFFSET = 8;
    constexpr float MS_PER_SECOND = 1000.0f;

    Rect Panel() { return { PANEL_X, PANEL_Y, PANEL_W, PANEL_H }; }

    Rect AcceptButton()
    {
        return { PANEL_X + (PANEL_W - (2 * BUTTON_W + BUTTON_GAP)) / 2, PANEL_Y + BUTTONS_Y, BUTTON_W, BUTTON_H };
    }

    Rect DeclineButton()
    {
        const Rect accept = AcceptButton();
        return { accept.x + BUTTON_W + BUTTON_GAP, accept.y, BUTTON_W, BUTTON_H };
    }
}

void CMobaRoomInviteWindow::Create(CNewUIManager* pNewUIMng)
{
    static CMobaRoomInviteWindow s_window;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_window);
}

bool CMobaRoomInviteWindow::Render()
{
    const Room::Invite& invite = Room::CurrentInvite();
    if (!invite.active)
        return true;

    const DWORD backColor = g_pRenderText->GetTextColor();
    Draw::FillRect(Panel(), 0.04f, 0.055f, 0.086f, 1.f);
    Draw::FrameRect(Panel(), 0.78f, 0.64f, 0.29f, 1.f);
    EndRenderColor();

    wchar_t line[160] = {};
    const int seconds = static_cast<int>(std::ceil(Room::InviteRemainingMs() / MS_PER_SECOND));
    swprintf(line, std::size(line), L"INVITACI\u00d3N A UNA SALA (%d s)", seconds);
    Draw::PutTextCentered(Draw::Bold(), PANEL_X, PANEL_Y + TITLE_Y, PANEL_W, line, 241, 196, 83);

    swprintf(line, std::size(line), L"%ls te invita a:", invite.from.c_str());
    Draw::PutTextCentered(Draw::Body(), PANEL_X, PANEL_Y + FROM_Y, PANEL_W, line, 233, 228, 212);
    Draw::PutTextCentered(Draw::Bold(), PANEL_X, PANEL_Y + ROOM_Y, PANEL_W, invite.roomName.c_str(), 140, 190, 255);

    Draw::Button(AcceptButton(), L"ACEPTAR", true);
    Draw::Button(DeclineButton(), L"RECHAZAR", false);
    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaRoomInviteWindow::Update()
{
    return true;
}

bool CMobaRoomInviteWindow::UpdateMouseEvent()
{
    if (Draw::HeldClickSwallowed())
        return false;

    if (!Room::CurrentInvite().active || !Draw::MouseInRect(Panel()))
        return true;

    if (IsPress(VK_LBUTTON))
    {
        Draw::SwallowHeldClick();
        if (Draw::MouseInRect(AcceptButton()))
            Room::AnswerInvite(true);
        else if (Draw::MouseInRect(DeclineButton()))
            Room::AnswerInvite(false);
    }

    // The panel swallows the mouse: a click on it must not walk the hero.
    return false;
}

bool CMobaRoomInviteWindow::UpdateKeyEvent()
{
    // Enter is left alone: it opens the chat. The popup is answered with the mouse only.
    return true;
}

float CMobaRoomInviteWindow::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaRoomInviteWindow::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
