#include "stdafx.h"
#include "UI/NewUI/HUD/Minimap/MobaMinimapFog.h"

#include "Render/Core/ImmediateRenderer.h"
#include "Render/RHI/RHI.h"
#include "World/Moba/MobaFog.h"

extern double WorldTime;

namespace UI::Minimap::MobaFog
{

namespace
{
    namespace Fog = ::World::Moba::Fog;

    constexpr int GRID = Fog::GridSize;
    constexpr int TEXEL_COUNT = GRID * GRID;

    // LoL-style look: tiles in full fog get a dark navy shade, tiles in sight a faint light wash so the
    // lit circles stand out. Alphas are before the map opacity option.
    constexpr float FULL_FOG_ALPHA = 0.43f;
    constexpr float SEEN_LIGHT_ALPHA = 0.07f;
    constexpr float FOG_R = 10.f, FOG_G = 16.f, FOG_B = 34.f;          // navy shade
    constexpr float SEEN_R = 255.f, SEEN_G = 245.f, SEEN_B = 220.f;   // warm light
    constexpr float ALPHA_SCALE = 255.f;
    constexpr float ROUND_HALF = 0.5f;
    constexpr int ALPHA_SHIFT = 24;   // RGBA bytes as a little-endian uint: 0xAABBGGRR

    RHI::TextureHandle s_texture{};
    unsigned int s_texels[TEXEL_COUNT];
    unsigned int s_builtFogVersion = 0;
    unsigned int s_builtClearVersion = 0;
    double s_lastBuildTime = 0.0;

    // Same texel orientation as the arena art (UpdateBlockedTexture in NewUIMiniMap.cpp): row = tile X,
    // column = tile Y, because the art's (u, v) are (tile Y, tile X).
    void BuildTexels(const float* fog, const unsigned char* clearCells)
    {
        for (int x = 0; x < GRID; ++x)
        {
            unsigned int* row = s_texels + x * GRID;
            for (int y = 0; y < GRID; ++y)
            {
                const int cell = y * GRID + x;
                if (clearCells[cell])
                {
                    row[y] = 0;
                    continue;
                }
                const float f = fog[cell];
                const float alpha = SEEN_LIGHT_ALPHA + (FULL_FOG_ALPHA - SEEN_LIGHT_ALPHA) * f;
                const unsigned int r = (unsigned int)(SEEN_R + (FOG_R - SEEN_R) * f + ROUND_HALF);
                const unsigned int g = (unsigned int)(SEEN_G + (FOG_G - SEEN_G) * f + ROUND_HALF);
                const unsigned int b = (unsigned int)(SEEN_B + (FOG_B - SEEN_B) * f + ROUND_HALF);
                row[y] = ((unsigned int)(alpha * ALPHA_SCALE + ROUND_HALF) << ALPHA_SHIFT) | (b << 16) | (g << 8) | r;
            }
        }
    }

    // The texture is up to date, or the fog changed less than a sight interval after the last build (the
    // fog eases every frame; the minimap follows it at the sight rate).
    bool TextureIsCurrent(unsigned int clearCellsVersion)
    {
        if (!s_texture.IsValid() || clearCellsVersion != s_builtClearVersion)
            return false;
        if (Fog::Version() == s_builtFogVersion)
            return true;

        const double sinceBuild = WorldTime - s_lastBuildTime;
        return sinceBuild >= 0.0 && sinceBuild < Fog::SightIntervalMs;
    }

    bool RefreshTexture(const unsigned char* clearCells, unsigned int clearCellsVersion)
    {
        if (TextureIsCurrent(clearCellsVersion))
            return true;

        BuildTexels(Fog::Cells(), clearCells);
        if (!s_texture.IsValid())
        {
            // CreateTexture binds the new texture without drawing the pending IR batch first (UpdateTexture
            // does), so the map art queued just before would come out with the fog texture.
            IR::Flush(IR::FlushCause::Texture);
            const RHI::TextureDesc desc{ GRID, GRID, RHI::TexFilter::Linear, RHI::TexWrap::Clamp };
            s_texture = RHI::CreateTexture(desc, s_texels);
        }
        else
        {
            RHI::UpdateTexture(s_texture, 0, 0, GRID, GRID, s_texels);
        }

        s_builtFogVersion = Fog::Version();
        s_builtClearVersion = clearCellsVersion;
        s_lastBuildTime = WorldTime;
        return s_texture.IsValid();
    }
}

void Render(const ArtPlacement& art, const unsigned char* clearCells, unsigned int clearCellsVersion)
{
    if (!Fog::IsActive() || clearCells == nullptr)
        return;
    // A refresh binds this texture behind BindTexture's cache; the draw right after goes through BindTexture,
    // which puts the cache back in step.
    if (!RefreshTexture(clearCells, clearCellsVersion))
        return;

    // Plain alpha blending without the alpha test, which would drop the light fog (alpha under its ref).
    EnableAlphaBlend3();
    glColor4f(1.f, 1.f, 1.f, art.opacity);
    RenderBitmapRotate(-(int)s_texture.id, art.centreX, art.centreY, art.width, art.height, art.rotation, art.u0, art.v0, art.uw, art.vh);
}

}
