#include "stdafx.h"
#include "UI/NewUI/Dialogs/MobaPingArt.h"

#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Textures/ZzzTexture.h"
#include "Core/Globals/_TextureIndex.h"

namespace UI::Moba::PingArt
{

namespace
{
    // The atlas (512 x 256): 8 x 4 cells of 64; the damage wedge is the 128 x 128 region at (384, 128).
    constexpr float TEXTURE_W = 512.f;
    constexpr float TEXTURE_H = 256.f;
    constexpr float CELL_SIZE = 64.f;
    constexpr int COLUMNS = 8;
    constexpr float WEDGE_X = 384.f;
    constexpr float WEDGE_Y = 128.f;
    constexpr float WEDGE_SIZE = 128.f;

    // The bitmap slot: the one after the minimap's block (BITMAP_MINI_MAP_END is not used by anything else).
    constexpr int PING_BITMAP = BITMAP_MINI_MAP_END;

    bool s_tried = false;
    bool s_loaded = false;

    // Colours of the rings of the ten icons (same as tools/make_ping_atlas.py).
    constexpr Tint KIND_TINT[World::Moba::Ping::KIND_COUNT + 1] = {
        { 1.f, 1.f, 1.f },
        { 125 / 255.f, 249 / 255.f, 1.f }, { 1.f, 205 / 255.f, 70 / 255.f }, { 1.f, 95 / 255.f, 85 / 255.f }, { 95 / 255.f, 165 / 255.f, 1.f },
        { 100 / 255.f, 235 / 255.f, 125 / 255.f }, { 1.f, 125 / 255.f, 45 / 255.f }, { 1.f, 107 / 255.f, 214 / 255.f }, { 85 / 255.f, 232 / 255.f, 232 / 255.f },
        { 1.f, 165 / 255.f, 60 / 255.f }, { 1.f, 70 / 255.f, 145 / 255.f },
    };

    // Height of a quad <width> ref px wide that is square on screen.
    float SquareHeight(float width)
    {
        return width * g_fScreenRate_x / g_fScreenRate_y;
    }

    // The renderer turns counter-clockwise; the art turns clockwise (compass).
    float RendererDegrees(float compass)
    {
        return -compass;
    }

    void DrawRegion(float cx, float cy, float width, float compass, float u, float v, float uw, float vh, Tint tint, float alpha)
    {
        glColor4f(tint.r, tint.g, tint.b, alpha);
        RenderBitmapRotate(PING_BITMAP, cx, cy, width, SquareHeight(width), RendererDegrees(compass), u, v, uw, vh);
    }
}

bool Loaded()
{
    if (!s_tried)
    {
        s_tried = true;
        s_loaded = LoadBitmap(L"Interface\\MobaPings.tga", PING_BITMAP, GL_LINEAR, GL_CLAMP_TO_EDGE, false);
    }
    return s_loaded;
}

Tint KindColor(World::Moba::Ping::Kind kind)
{
    return kind >= World::Moba::Ping::KIND_GENERIC && kind <= World::Moba::Ping::KIND_COUNT ? KIND_TINT[kind] : KIND_TINT[0];
}

void Begin()
{
    EnableAlphaBlend3();
}

void End()
{
    glColor4f(1.f, 1.f, 1.f, 1.f);
    DisableAlphaBlend();
}

void DrawCell(int cell, float cx, float cy, float width, float compass, Tint tint, float alpha)
{
    if (!Loaded())
        return;

    // Half a texel is trimmed on each side so the linear filter never pulls in the next cell.
    constexpr float PAD_U = 0.5f / TEXTURE_W;
    constexpr float PAD_V = 0.5f / TEXTURE_H;
    const float cellU = CELL_SIZE / TEXTURE_W;
    const float cellV = CELL_SIZE / TEXTURE_H;
    const float u = static_cast<float>(cell % COLUMNS) * cellU + PAD_U;
    const float v = static_cast<float>(cell / COLUMNS) * cellV + PAD_V;
    DrawRegion(cx, cy, width, compass, u, v, cellU - 2.f * PAD_U, cellV - 2.f * PAD_V, tint, alpha);
}

void DrawKind(World::Moba::Ping::Kind kind, float cx, float cy, float width, float alpha)
{
    if (kind < World::Moba::Ping::KIND_GENERIC || kind > World::Moba::Ping::KIND_COUNT)
        return;

    DrawCell(kind - 1, cx, cy, width, 0.f, { 1.f, 1.f, 1.f }, alpha);
}

void DrawWedge(float cx, float cy, float width, float compass, Tint tint, float alpha)
{
    if (!Loaded())
        return;

    DrawRegion(cx, cy, width, compass, WEDGE_X / TEXTURE_W, WEDGE_Y / TEXTURE_H, WEDGE_SIZE / TEXTURE_W, WEDGE_SIZE / TEXTURE_H, tint, alpha);
}

}
