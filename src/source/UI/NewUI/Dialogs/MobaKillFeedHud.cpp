#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaKillFeedHud.h"

#include "UI/NewUI/NewUIManager.h"
#include "UI/Legacy/UIControls.h"             // g_pRenderText
#include "UI/NewUI/Dialogs/MobaPopupDraw.h"
#include "World/Moba/MobaAnnouncer.h"

#include <algorithm>

using namespace SEASON3B;
namespace Announcer = World::Moba::Announcer;
namespace Draw = UI::Moba::Draw;

extern double WorldTime;

namespace
{
    // Layout, in the 640x480 reference screen: right aligned in the upper right corner.
    constexpr int FEED_RIGHT = 634;
    constexpr int FEED_TOP = 52;
    constexpr int FEED_WIDTH = 250;
    constexpr int LINE_GAP = 2;

    constexpr float LAYER_DEPTH = 10.2f;
    constexpr float KEY_EVENT_ORDER = 50.0f;
    constexpr DWORD UI_KEY_OFFSET = 12;

    struct Rgb
    {
        BYTE r, g, b;
    };

    constexpr Rgb TEAM_COLORS[3] = { { 225, 225, 225 }, { 120, 170, 255 }, { 255, 130, 120 } };

    // 1 while the line is fresh, down to 0 while it fades out.
    float LineBrightness(const Announcer::FeedLine& line)
    {
        const double left = Announcer::FEED_LINE_MS - (WorldTime - line.atMs);
        return static_cast<float>(std::clamp(left / Announcer::FEED_FADE_MS, 0.0, 1.0));
    }

    BYTE Dim(BYTE value, float brightness)
    {
        return static_cast<BYTE>(value * brightness);
    }
}

void CMobaKillFeedHud::Create(CNewUIManager* pNewUIMng)
{
    static CMobaKillFeedHud s_hud;
    if (pNewUIMng != nullptr)
        pNewUIMng->AddUIObj(INTERFACE_END + UI_KEY_OFFSET, &s_hud);
}

bool CMobaKillFeedHud::Render()
{
    const std::vector<Announcer::FeedLine>& feed = Announcer::Feed();
    if (feed.empty())
        return true;

    const DWORD backColor = g_pRenderText->GetTextColor();
    const HFONT font = Draw::Body();
    const int lineHeight = Draw::BodyLineHeight() + LINE_GAP;
    int y = FEED_TOP;
    for (const Announcer::FeedLine& line : feed)
    {
        const Rgb color = TEAM_COLORS[std::clamp(line.team, 0, 2)];
        const float brightness = LineBrightness(line);
        Draw::PutText(font, FEED_RIGHT - FEED_WIDTH, y, FEED_WIDTH, line.text.c_str(), Dim(color.r, brightness), Dim(color.g, brightness), Dim(color.b, brightness), RT3_SORT_RIGHT);
        y += lineHeight;
    }

    g_pRenderText->SetTextColor(backColor);
    return true;
}

bool CMobaKillFeedHud::Update()
{
    Announcer::Pump();
    return true;
}

bool CMobaKillFeedHud::UpdateMouseEvent()
{
    return true;
}

bool CMobaKillFeedHud::UpdateKeyEvent()
{
    return true;
}

float CMobaKillFeedHud::GetLayerDepth()
{
    return LAYER_DEPTH;
}

float CMobaKillFeedHud::GetKeyEventOrder()
{
    return KEY_EVENT_ORDER;
}
