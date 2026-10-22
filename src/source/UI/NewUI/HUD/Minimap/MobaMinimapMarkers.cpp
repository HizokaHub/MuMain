#include "stdafx.h"
#include "World/Moba/MobaMinimapCreeps.h"
#include "UI/NewUI/HUD/Minimap/MobaMinimapMarkers.h"

#include "Character/CharacterManager.h"   // gCharacterManager (own hero's base class)
#include "Engine/Object/ZzzCharacter.h"    // Hero
#include "Network/Server/WSclient.h"       // minimap feed, fountains, channel bar, g_MyMobaTeam
#include "Camera/CameraManager.h"          // g_Camera
#include "Camera/CameraProjection.h"       // ScreenToWorldRay
#include "Render/Textures/ZzzOpenglUtil.h" // MousePosition (the origin of the last ray), RenderColor
#include "UI/Legacy/UIControls.h"          // g_pRenderText

#include <algorithm>
#include <cmath>

extern const int g_MobaTotemCount;
extern const int g_MobaTotems[][3];   // MapManager.cpp: tile x, tile y, colour (1 blue, 2 green, 3 red, 4 yellow)

namespace UI::Minimap::MobaMarkers
{

namespace
{
    constexpr float TWO_PI = 6.28318531f;
    constexpr float MAP_TILES = 256.f;

    // ---- Atlases (cell layout shared with the asset generator) ----

    struct AtlasGrid
    {
        int bitmap;
        int cols, rows;
        float texW, texH;
    };

    // MobaPortraits: 512x128, 8x2 cells of 64 px.
    constexpr int PORTRAIT_COLS = 8;
    constexpr int PORTRAIT_ROWS = 2;
    constexpr float PORTRAIT_TEX_W = 512.f;
    constexpr float PORTRAIT_TEX_H = 128.f;
    constexpr int PORTRAIT_CLASS_COUNT = 7;   // cells 0..6: colour portraits by base class index
    constexpr int PORTRAIT_RING = 7;          // white ring (tinted)
    constexpr int PORTRAIT_GREY_FIRST = 8;    // cells 8..14: the same portraits in grey (dead)
    constexpr int PORTRAIT_DISC = 15;         // white filled disc (tinted)

    // MobaMinimapIcons: 256x256, 8x8 cells of 32 px.
    constexpr int ICON_COLS = 8;
    constexpr int ICON_ROWS = 8;
    constexpr float ICON_TEX_SIZE = 256.f;
    constexpr int ICON_TOTEM_FIRST = 0;       // 0 blue, 1 green, 2 red, 3 yellow (pre-coloured)
    constexpr int ICON_GLOW = 4;
    constexpr int ICON_CONTESTED = 5;
    constexpr int ICON_FOUNTAIN_NEUTRAL = 8;
    constexpr int ICON_FOUNTAIN_BLUE = 9;
    constexpr int ICON_FOUNTAIN_RED = 10;
    constexpr int ICON_JUNGLE_ARCH = 11;
    constexpr int ICON_TURRET = 12;
    constexpr int ICON_NEXUS = 13;
    constexpr int ICON_WARD = 14;
    constexpr int ICON_DOT = 15;
    constexpr int ICON_DEAD_X = 16;
    constexpr int ICON_THIN_RING = 17;

    // Share of its cell a drawing covers (to size a quad from the size the drawing should have).
    constexpr float JUNGLE_ARCH_CELL_FILL = 20.f / 32.f;
    constexpr float DOT_CELL_FILL = 12.f / 32.f;
    constexpr float THIN_RING_CELL_FILL = 31.f / 32.f;

    // ---- Feed / game constants ----

    constexpr BYTE KIND_CHAMPION = 1;
    constexpr BYTE KIND_TURRET = 2;
    constexpr BYTE KIND_NEXUS = 3;
    constexpr BYTE KIND_WARD = 4;
    constexpr BYTE TEAM_BLUE_ID = 1;
    constexpr BYTE TEAM_RED_ID = 2;
    constexpr int CHANNEL_RECALL = 1;                       // g_MobaChannelKind
    constexpr float FOUNTAIN_HEAL_RADIUS_TILES = 12.f;      // MobaFountains.HealRadius on the server
    constexpr int LOW_HP_PERCENT = 25;

    // ---- Colours ----

    struct Rgb
    {
        float r, g, b;
    };

    constexpr Rgb COLOR_TEAM_BLUE = { 70 / 255.f, 150 / 255.f, 1.f };
    constexpr Rgb COLOR_TEAM_RED = { 1.f, 75 / 255.f, 65 / 255.f };
    constexpr Rgb COLOR_HERO_GOLD = { 1.f, 205 / 255.f, 70 / 255.f };
    constexpr Rgb COLOR_RECALL = { 120 / 255.f, 230 / 255.f, 1.f };
    constexpr Rgb COLOR_WARD = { 0.35f, 1.f, 0.35f };
    constexpr Rgb COLOR_DEAD = { 0.55f, 0.55f, 0.55f };
    constexpr Rgb COLOR_TRACK = { 0.08f, 0.08f, 0.1f };
    constexpr Rgb COLOR_WHITE = { 1.f, 1.f, 1.f };
    constexpr Rgb COLOR_BLACK = { 0.f, 0.f, 0.f };
    constexpr Rgb COLOR_JUNGLE_FLAT = { 0.1f, 0.55f, 1.f };
    constexpr Rgb COLOR_FOUNTAIN_NEUTRAL_FLAT = { 0.55f, 0.55f, 0.6f };

    // Totems by their g_MobaTotems colour index (0 unused).
    constexpr int TOTEM_COLOR_COUNT = 5;
    constexpr Rgb TOTEM_COLORS[TOTEM_COLOR_COUNT] = {
        { 1.f, 0.6f, 0.4f }, { 0.25f, 0.5f, 1.f }, { 0.3f, 1.f, 0.35f }, { 1.f, 0.25f, 0.2f }, { 1.f, 0.9f, 0.25f } };
    const wchar_t* const TOTEM_NAMES[TOTEM_COLOR_COUNT] = {
        L"", L"T\u00f3tem azul", L"T\u00f3tem verde", L"T\u00f3tem rojo", L"T\u00f3tem amarillo" };

    // ---- Sizes: reference px at 100 % markers (1 ref px is ~2.1 screen px at 1365 px wide) ----

    constexpr float JUNGLE_ARCH_SIZE = 3.3f;   // the arch itself, not its cell
    constexpr float TOTEM_SIZE = 7.5f;
    constexpr float FOUNTAIN_SIZE = 8.4f;
    constexpr float TURRET_SIZE = 7.f;
    constexpr float NEXUS_SIZE = 11.f;
    constexpr float WARD_SIZE = 5.5f;
    constexpr float CHAMPION_SIZE = 9.4f;
    constexpr float HERO_SIZE = 11.3f;

    // The old flat squares, used when an atlas is missing.
    constexpr float FLAT_JUNGLE_SIZE = 4.5f;
    constexpr float FLAT_TOTEM_SIZE = 7.f;
    constexpr float FLAT_TOTEM_BORDER = 1.4f;
    constexpr float FLAT_FOUNTAIN_SIZE = 9.f;
    constexpr float FLAT_FOUNTAIN_BORDER = 2.f;
    constexpr float FLAT_CHAMPION_SIZE = 6.f;
    constexpr float FLAT_STRUCTURE_SIZE = 7.f;
    constexpr float FLAT_NEXUS_SIZE = 11.f;
    constexpr float FLAT_WARD_SIZE = 5.f;

    // ---- Look and animation (scales are fractions of the marker's width) ----

    // Jungle entrances fade in as the hero gets close.
    constexpr float JUNGLE_ALPHA_DISTANT = 0.5f;
    constexpr float JUNGLE_ALPHA_CLOSE = 0.9f;
    constexpr float JUNGLE_CLOSE_TILES = 30.f;     // full alpha from this hero distance in
    constexpr float JUNGLE_DISTANT_TILES = 50.f;   // base alpha from this hero distance out

    // Totem glow: drawn behind the totem, tinted, flickering a little (each totem out of phase).
    constexpr float TOTEM_GLOW_SCALE = 1.9f;
    constexpr float TOTEM_GLOW_ALPHA = 0.8f;
    constexpr float TOTEM_GLOW_WHITEN = 0.2f;      // mixed towards white so it reads as light
    constexpr float TOTEM_FLICKER_DEPTH = 0.3f;
    constexpr float TOTEM_FLICKER_SIZE = 0.06f;
    constexpr double TOTEM_FLICKER_SLOW_HZ = 1.3;
    constexpr double TOTEM_FLICKER_FAST_HZ = 3.7;
    constexpr float TOTEM_PHASE_STEP = 1.9f;       // radians between consecutive totems

    // Champion token.
    constexpr float BACKING_SCALE = 1.08f;
    constexpr float BACKING_ALPHA = 0.8f;
    constexpr float UNKNOWN_CLASS_DISC_SCALE = 0.9f;
    constexpr float DEAD_PORTRAIT_SHADE = 0.6f;    // the grey cells are not darkened in the atlas
    constexpr float DEAD_X_SCALE = 0.8f;
    constexpr double LOW_HP_BLINK_HZ = 2.0;
    constexpr float RECALL_RING_SCALE = 1.22f;
    constexpr float RECALL_RING_GROW = 0.1f;
    constexpr float RECALL_ALPHA_MIN = 0.45f;
    constexpr double RECALL_PULSE_HZ = 2.0;

    // Arcs drawn as dots (fountain capture, own recall).
    constexpr int DOTS_PER_TURN = 24;
    constexpr float TRACK_ALPHA = 0.75f;
    constexpr float CAPTURE_RING_RADIUS = 0.64f;
    constexpr float CAPTURE_DOT_SIZE = 0.17f;
    constexpr float CONTESTED_BADGE_SIZE = 0.62f;
    constexpr float CONTESTED_BADGE_TURN = 0.125f;   // top-right
    constexpr double CONTESTED_FLASH_HZ = 4.0;
    constexpr float CONTESTED_FLASH_WHITEN = 0.65f;
    constexpr float HERO_ARC_RADIUS = 0.64f;
    constexpr float HERO_ARC_DOT_SIZE = 0.13f;

    // Heal zone of an owned fountain.
    constexpr float HEAL_ZONE_ALPHA = 0.25f;
    constexpr float HEAL_ZONE_BREATH = 0.08f;        // +- alpha
    constexpr double HEAL_ZONE_BREATH_HZ = 0.5;

    // Totem name next to the hovered totem.
    constexpr float TOOLTIP_GAP = 2.f;
    constexpr float TOOLTIP_HALF_HEIGHT = 5.f;
    constexpr BYTE TOOLTIP_BG_ALPHA = 180;

    // ---- Helpers ----

    AtlasGrid PortraitGrid(const Atlases& a)
    {
        return { a.portraitBitmap, PORTRAIT_COLS, PORTRAIT_ROWS, PORTRAIT_TEX_W, PORTRAIT_TEX_H };
    }

    AtlasGrid IconGrid(const Atlases& a)
    {
        return { a.iconBitmap, ICON_COLS, ICON_ROWS, ICON_TEX_SIZE, ICON_TEX_SIZE };
    }

    void SetColor(const Rgb& c, float alpha = 1.f)
    {
        glColor4f(c.r, c.g, c.b, alpha);
    }

    Rgb Whiten(const Rgb& c, float t)
    {
        return { c.r + (1.f - c.r) * t, c.g + (1.f - c.g) * t, c.b + (1.f - c.b) * t };
    }

    Rgb TeamColor(BYTE team)
    {
        return team == TEAM_BLUE_ID ? COLOR_TEAM_BLUE : COLOR_TEAM_RED;
    }

    bool IsTeam(BYTE team)
    {
        return team == TEAM_BLUE_ID || team == TEAM_RED_ID;
    }

    bool IsAlly(const MobaMinimapEntry& e)
    {
        return g_MyMobaTeam != 0 && e.team == g_MyMobaTeam;
    }

    bool IsValidTotemColor(int colour)
    {
        return colour >= 1 && colour < TOTEM_COLOR_COUNT;
    }

    float Percent(BYTE value)
    {
        return std::min((float)value, 100.f) / 100.f;
    }

    // 0..1 sine wave at hz (WorldTime is in ms).
    float Wave(double hz, float phase = 0.f)
    {
        const double cycle = fmod(WorldTime * 0.001 * hz, 1.0);
        return 0.5f + 0.5f * sinf((float)cycle * TWO_PI + phase);
    }

    // On during the first half of every period.
    bool Blink(double hz)
    {
        return fmod(WorldTime * 0.001 * hz, 1.0) < 0.5;
    }

    float SmoothStep(float t)
    {
        t = std::clamp(t, 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    }

    // Height of a quad w ref px wide that is square on screen.
    float SquareHeight(const Frame& f, float w)
    {
        return w * f.sx / f.sy;
    }

    // One atlas cell centred on (mx, my), w ref px wide, upright and square on screen. Half a texel
    // is trimmed on each side so GL_LINEAR never pulls in the neighbouring cell.
    void DrawCell(const Frame& f, const AtlasGrid& g, int cell, float mx, float my, float w)
    {
        const float cellU = 1.f / (float)g.cols;
        const float cellV = 1.f / (float)g.rows;
        const float padU = 0.5f / g.texW;
        const float padV = 0.5f / g.texH;
        const float u = (float)(cell % g.cols) * cellU + padU;
        const float v = (float)(cell / g.cols) * cellV + padV;
        RenderBitmapRotate(g.bitmap, mx, my, w, SquareHeight(f, w), 0.f, u, v, cellU - 2.f * padU, cellV - 2.f * padV);
    }

    // A flat square centred on (mx, my) in the current colour (old look, atlas missing).
    void FlatSquare(const Frame& f, float mx, float my, float w)
    {
        const float h = SquareHeight(f, w);
        RenderColor(mx - w / 2.f, my - h / 2.f, w, h);
    }

    // The point `turn` of a circle around (mx, my), clockwise from 12 o'clock, round on screen.
    void PointOnCircle(const Frame& f, float mx, float my, float radius, float turn, float& px, float& py)
    {
        const float angle = turn * TWO_PI;
        px = mx + sinf(angle) * radius;
        py = my - cosf(angle) * SquareHeight(f, radius);
    }

    // `turns` of a dotted circle around (mx, my) from 12 o'clock (negative = counter-clockwise),
    // DOTS_PER_TURN dots per full turn; the last dot fades in with the share of it that is covered.
    void DrawDotArc(const Frame& f, const AtlasGrid& icons, float mx, float my, float radius, float dotSize,
        float turns, const Rgb& c, float alpha)
    {
        const float direction = turns < 0.f ? -1.f : 1.f;
        const float covered = fabsf(turns) * (float)DOTS_PER_TURN;
        for (int k = 0; k < DOTS_PER_TURN && (float)k < covered; ++k)
        {
            float px, py;
            PointOnCircle(f, mx, my, radius, direction * ((float)k + 0.5f) / (float)DOTS_PER_TURN, px, py);
            SetColor(c, alpha * std::min(1.f, covered - (float)k));
            DrawCell(f, icons, ICON_DOT, px, py, dotSize / DOT_CELL_FILL);
        }
    }

    // ---- Jungle entrances ----

    // 0 far from the hero .. 1 close to it.
    float HeroCloseness(int tileX, int tileY)
    {
        if (Hero == nullptr)
            return 0.f;
        const float dist = hypotf((float)(tileX - Hero->PositionX), (float)(tileY - Hero->PositionY));
        return SmoothStep((JUNGLE_DISTANT_TILES - dist) / (JUNGLE_DISTANT_TILES - JUNGLE_CLOSE_TILES));
    }

    void RenderJungleEntrancesFlat(const Frame& f, const std::vector<std::pair<int, int>>& doors)
    {
        SetColor(COLOR_JUNGLE_FLAT);
        for (const auto& d : doors)
        {
            float mx, my;
            if (!f.TileToRef((float)d.first, (float)d.second, mx, my))
                continue;
            FlatSquare(f, mx, my, FLAT_JUNGLE_SIZE * f.markers);
        }
        EndRenderColor();
    }

    // ---- Totems ----

    float TotemFlicker(int totem)
    {
        const float phase = (float)totem * TOTEM_PHASE_STEP;
        const float wobble = 0.5f * (Wave(TOTEM_FLICKER_SLOW_HZ, phase) + Wave(TOTEM_FLICKER_FAST_HZ, 2.f * phase));
        return 1.f - TOTEM_FLICKER_DEPTH * wobble;
    }

    void RenderTotemsFlat(const Frame& f)
    {
        const float w = FLAT_TOTEM_SIZE * f.markers;
        for (int k = 0; k < g_MobaTotemCount; ++k)
        {
            const int colour = g_MobaTotems[k][2];
            if (!IsValidTotemColor(colour))
                continue;
            float mx, my;
            if (!f.TileToRef((float)g_MobaTotems[k][0], (float)g_MobaTotems[k][1], mx, my))
                continue;
            SetColor(COLOR_BLACK);
            FlatSquare(f, mx, my, w + FLAT_TOTEM_BORDER);
            SetColor(TOTEM_COLORS[colour]);
            FlatSquare(f, mx, my, w);
        }
        EndRenderColor();
    }

    int HoveredTotem(const Frame& f, float w)
    {
        const float h = SquareHeight(f, w);
        for (int k = 0; k < g_MobaTotemCount; ++k)
        {
            if (!IsValidTotemColor(g_MobaTotems[k][2]))
                continue;
            float mx, my;
            if (!f.TileToRef((float)g_MobaTotems[k][0], (float)g_MobaTotems[k][1], mx, my))
                continue;
            if (fabsf((float)MouseX - mx) <= w / 2.f && fabsf((float)MouseY - my) <= h / 2.f)
                return k;
        }
        return -1;
    }

    // ---- Fountains ----

    int FountainCell(BYTE owner)
    {
        if (owner == TEAM_BLUE_ID)
            return ICON_FOUNTAIN_BLUE;
        if (owner == TEAM_RED_ID)
            return ICON_FOUNTAIN_RED;
        return ICON_FOUNTAIN_NEUTRAL;
    }

    void RenderFountainsFlat(const Frame& f)
    {
        const float w = FLAT_FOUNTAIN_SIZE * f.markers;
        for (int k = 0; k < MOBA_FOUNTAIN_COUNT; ++k)
        {
            float mx, my;
            if (!f.TileToRef((float)g_MobaFountains[k][0], (float)g_MobaFountains[k][1], mx, my))
                continue;
            const BYTE owner = g_MobaFountainInfo[k].owner;
            SetColor(COLOR_WHITE);
            FlatSquare(f, mx, my, w + FLAT_FOUNTAIN_BORDER);
            SetColor(IsTeam(owner) ? TeamColor(owner) : COLOR_FOUNTAIN_NEUTRAL_FLAT);
            FlatSquare(f, mx, my, w);
        }
        EndRenderColor();
    }

    // Faint circle the size of the heal area, in the owner's colour, breathing slowly.
    void DrawHealZone(const Frame& f, const AtlasGrid& icons, float mx, float my, BYTE owner)
    {
        if (f.zoom > 1.01f)
            return;   // a zoomed circle would overflow the minimap square (no clipping available)
        const float radius = f.TilesToRef(FOUNTAIN_HEAL_RADIUS_TILES);
        const float breath = 2.f * Wave(HEAL_ZONE_BREATH_HZ) - 1.f;
        SetColor(TeamColor(owner), HEAL_ZONE_ALPHA + HEAL_ZONE_BREATH * breath);
        DrawCell(f, icons, ICON_THIN_RING, mx, my, 2.f * radius / THIN_RING_CELL_FILL);
    }

    // Capture progress around the basin: blue fills the right half clockwise from 12 to 6 o'clock,
    // red the left half counter-clockwise. A contested fountain flashes white and shows crossed swords.
    void DrawCaptureRing(const Frame& f, const AtlasGrid& icons, const MobaFountainInfo& info, float mx, float my, float w)
    {
        if (info.bluePct == 0 && info.redPct == 0 && !info.contested)
            return;

        const float radius = w * CAPTURE_RING_RADIUS;
        const float dot = w * CAPTURE_DOT_SIZE;
        const bool flash = info.contested && Blink(CONTESTED_FLASH_HZ);
        const float whiten = flash ? CONTESTED_FLASH_WHITEN : 0.f;
        DrawDotArc(f, icons, mx, my, radius, dot, 1.f, flash ? COLOR_WHITE : COLOR_TRACK, TRACK_ALPHA);
        DrawDotArc(f, icons, mx, my, radius, dot, 0.5f * Percent(info.bluePct), Whiten(COLOR_TEAM_BLUE, whiten), 1.f);
        DrawDotArc(f, icons, mx, my, radius, dot, -0.5f * Percent(info.redPct), Whiten(COLOR_TEAM_RED, whiten), 1.f);
        if (!info.contested)
            return;

        float bx, by;
        PointOnCircle(f, mx, my, radius, CONTESTED_BADGE_TURN, bx, by);
        SetColor(COLOR_WHITE);
        DrawCell(f, icons, ICON_CONTESTED, bx, by, w * CONTESTED_BADGE_SIZE);
    }

    // ---- Champions ----

    // Atlas index (0..6) of a base class, -1 if it has no portrait.
    int PortraitIndex(CLASS_TYPE baseClass)
    {
        switch (baseClass)
        {
        case CLASS_WIZARD:      return 0;
        case CLASS_KNIGHT:      return 1;
        case CLASS_ELF:         return 2;
        case CLASS_DARK:        return 3;   // Magic Gladiator
        case CLASS_DARK_LORD:   return 4;
        case CLASS_SUMMONER:    return 5;
        case CLASS_RAGEFIGHTER: return 6;
        default:                return -1;
        }
    }

    // Dark backing disc, portrait (grey when dead), tinted ring and, when dead, a red X.
    // A portrait index outside 0..6 draws a plain disc in the ring colour instead.
    void DrawChampionToken(const Frame& f, const Atlases& a, float mx, float my, float w, int portrait, bool dead, const Rgb& ring)
    {
        const AtlasGrid portraits = PortraitGrid(a);
        SetColor(COLOR_BLACK, BACKING_ALPHA);
        DrawCell(f, portraits, PORTRAIT_DISC, mx, my, w * BACKING_SCALE);
        if (portrait >= 0 && portrait < PORTRAIT_CLASS_COUNT)
        {
            if (dead)
                SetColor({ DEAD_PORTRAIT_SHADE, DEAD_PORTRAIT_SHADE, DEAD_PORTRAIT_SHADE });
            else
                SetColor(COLOR_WHITE);
            DrawCell(f, portraits, dead ? PORTRAIT_GREY_FIRST + portrait : portrait, mx, my, w);
        }
        else
        {
            SetColor(ring);
            DrawCell(f, portraits, PORTRAIT_DISC, mx, my, w * UNKNOWN_CLASS_DISC_SCALE);
        }
        SetColor(ring);
        DrawCell(f, portraits, PORTRAIT_RING, mx, my, w);
        if (dead && a.iconsLoaded)
        {
            SetColor(COLOR_WHITE);
            DrawCell(f, IconGrid(a), ICON_DEAD_X, mx, my, w * DEAD_X_SCALE);
        }
    }

    // Pulsing cyan ring outside the token of a champion channelling the recall.
    void DrawRecallPulse(const Frame& f, const Atlases& a, float mx, float my, float w)
    {
        const float pulse = Wave(RECALL_PULSE_HZ);
        SetColor(COLOR_RECALL, RECALL_ALPHA_MIN + (1.f - RECALL_ALPHA_MIN) * pulse);
        DrawCell(f, PortraitGrid(a), PORTRAIT_RING, mx, my, w * (RECALL_RING_SCALE + RECALL_RING_GROW * pulse));
    }

    void DrawFeedChampion(const Frame& f, const Atlases& a, const MobaMinimapEntry& e)
    {
        float mx, my;
        if (!f.TileToRef((float)e.x, (float)e.y, mx, my))
            return;
        const float w = CHAMPION_SIZE * f.markers;
        const bool dead = e.hp == 0;
        const bool lowHpFlash = !dead && IsAlly(e) && e.hp <= LOW_HP_PERCENT && Blink(LOW_HP_BLINK_HZ);
        const Rgb ring = dead ? COLOR_DEAD : lowHpFlash ? COLOR_WHITE : TeamColor(e.team);
        if (e.recalling && !dead)
            DrawRecallPulse(f, a, mx, my, w);
        DrawChampionToken(f, a, mx, my, w, e.cls, dead, ring);
    }

    // Champions are drawn in layers so the ones that matter most end on top.
    enum ChampionLayer
    {
        LAYER_DEAD,
        LAYER_ENEMY,
        LAYER_ALLY,
        LAYER_COUNT,
    };

    ChampionLayer LayerOf(const MobaMinimapEntry& e)
    {
        if (e.hp == 0)
            return LAYER_DEAD;
        return IsAlly(e) ? LAYER_ALLY : LAYER_ENEMY;
    }

    void DrawFeedStructure(const Frame& f, const AtlasGrid& icons, const MobaMinimapEntry& e)
    {
        int cell = 0;
        float size = 0.f;
        Rgb color = COLOR_WHITE;
        switch (e.kind)
        {
        case KIND_TURRET: cell = ICON_TURRET; size = TURRET_SIZE; color = TeamColor(e.team); break;
        case KIND_NEXUS:  cell = ICON_NEXUS;  size = NEXUS_SIZE;  color = TeamColor(e.team); break;
        case KIND_WARD:   cell = ICON_WARD;   size = WARD_SIZE;   color = COLOR_WARD;        break;
        default: return;
        }
        float mx, my;
        if (!f.TileToRef((float)e.x, (float)e.y, mx, my))
            return;
        SetColor(color);
        DrawCell(f, icons, cell, mx, my, size * f.markers);
    }

    // Old flat squares for the champions or for everything else in the feed.
    void RenderFeedFlat(const Frame& f, bool champions)
    {
        for (int m = 0; m < g_MobaMinimapCount; ++m)
        {
            const MobaMinimapEntry& e = g_MobaMinimap[m];
            if ((e.kind == KIND_CHAMPION) != champions)
                continue;
            float mx, my;
            if (!f.TileToRef((float)e.x, (float)e.y, mx, my))
                continue;
            SetEntryColor(e);
            FlatSquare(f, mx, my, EntrySize(e, FLAT_CHAMPION_SIZE, FLAT_STRUCTURE_SIZE, FLAT_NEXUS_SIZE, FLAT_WARD_SIZE) * f.markers);
        }
        EndRenderColor();
    }

    void RenderFeedStructures(const Frame& f, const Atlases& a)
    {
        if (!a.iconsLoaded)
        {
            RenderFeedFlat(f, false);
            return;
        }
        EnableAlphaBlend3();
        const AtlasGrid icons = IconGrid(a);
        for (int m = 0; m < g_MobaMinimapCount; ++m)
            DrawFeedStructure(f, icons, g_MobaMinimap[m]);
    }

    void RenderFeedChampions(const Frame& f, const Atlases& a)
    {
        if (!a.portraitsLoaded)
        {
            RenderFeedFlat(f, true);
            return;
        }
        EnableAlphaBlend3();
        for (int layer = 0; layer < LAYER_COUNT; ++layer)
        {
            for (int m = 0; m < g_MobaMinimapCount; ++m)
            {
                const MobaMinimapEntry& e = g_MobaMinimap[m];
                if (e.kind == KIND_CHAMPION && LayerOf(e) == layer)
                    DrawFeedChampion(f, a, e);
            }
        }
    }

    // Own recall channel: cyan dots filling clockwise from 12 o'clock with the real progress.
    void DrawOwnRecallProgress(const Frame& f, const Atlases& a, float mx, float my, float w)
    {
        if (!a.iconsLoaded || g_MobaLevel <= 0 || g_MobaChannelKind != CHANNEL_RECALL || WorldTime >= g_MobaChannelEnd)
            return;

        const double total = g_MobaChannelEnd - g_MobaChannelStart;
        const float progress = total > 1.0 ? std::clamp((float)((WorldTime - g_MobaChannelStart) / total), 0.f, 1.f) : 1.f;
        const AtlasGrid icons = IconGrid(a);
        const float radius = w * HERO_ARC_RADIUS;
        const float dot = w * HERO_ARC_DOT_SIZE;
        DrawDotArc(f, icons, mx, my, radius, dot, 1.f, COLOR_TRACK, TRACK_ALPHA);
        DrawDotArc(f, icons, mx, my, radius, dot, progress, COLOR_RECALL, 1.f);
    }
}

void Frame::ToRef(float pu, float pv, float& mx, float& my) const
{
    const float lx = (pu - centreU) * zoom * sidePx;
    const float ly = (centreV - pv) * zoom * vScale * sidePx;
    mx = centreX + (lx * cosRot - ly * sinRot) / sx;
    my = centreY - (lx * sinRot + ly * cosRot) / sy;
}

bool Frame::TileToRef(float tileX, float tileY, float& mx, float& my) const
{
    const float pu = tileY / MAP_TILES, pv = tileX / MAP_TILES;
    ToRef(pu, pv, mx, my);
    const float half = 0.5f * sidePx;
    return fabsf((pu - centreU) * zoom * sidePx) <= half && fabsf((centreV - pv) * zoom * vScale * sidePx) <= half;
}

float Frame::TilesToRef(float tiles) const
{
    return tiles / MAP_TILES * sidePx * zoom / sx;
}

void RenderJungleEntrances(const Frame& frame, const Atlases& atlases, const std::vector<std::pair<int, int>>& doors)
{
    if (!atlases.iconsLoaded)
    {
        RenderJungleEntrancesFlat(frame, doors);
        return;
    }

    EnableAlphaBlend3();
    const AtlasGrid icons = IconGrid(atlases);
    const float w = JUNGLE_ARCH_SIZE / JUNGLE_ARCH_CELL_FILL * frame.markers;
    for (const auto& d : doors)
    {
        float mx, my;
        if (!frame.TileToRef((float)d.first, (float)d.second, mx, my))
            continue;
        SetColor(COLOR_WHITE, JUNGLE_ALPHA_DISTANT + (JUNGLE_ALPHA_CLOSE - JUNGLE_ALPHA_DISTANT) * HeroCloseness(d.first, d.second));
        DrawCell(frame, icons, ICON_JUNGLE_ARCH, mx, my, w);
    }
    SetColor(COLOR_WHITE);
}

int RenderTotems(const Frame& frame, const Atlases& atlases)
{
    if (!atlases.iconsLoaded)
    {
        RenderTotemsFlat(frame);
        return HoveredTotem(frame, FLAT_TOTEM_SIZE * frame.markers);
    }

    // The glow is blended normally, not with EnableAlphaBlend: that one is GL_ONE/GL_ONE, which
    // ignores alpha and would show the glow cell's whole quad.
    EnableAlphaBlend3();
    const AtlasGrid icons = IconGrid(atlases);
    const float w = TOTEM_SIZE * frame.markers;
    for (int k = 0; k < g_MobaTotemCount; ++k)
    {
        const int colour = g_MobaTotems[k][2];
        if (!IsValidTotemColor(colour))
            continue;
        float mx, my;
        if (!frame.TileToRef((float)g_MobaTotems[k][0], (float)g_MobaTotems[k][1], mx, my))
            continue;
        const float flicker = TotemFlicker(k);
        SetColor(Whiten(TOTEM_COLORS[colour], TOTEM_GLOW_WHITEN), TOTEM_GLOW_ALPHA * flicker);
        DrawCell(frame, icons, ICON_GLOW, mx, my, w * TOTEM_GLOW_SCALE * (1.f - TOTEM_FLICKER_SIZE * (1.f - flicker)));
        SetColor(COLOR_WHITE);
        DrawCell(frame, icons, ICON_TOTEM_FIRST + colour - 1, mx, my, w);
    }
    return HoveredTotem(frame, w);
}

void RenderTotemTooltip(const Frame& frame, int totem)
{
    if (totem < 0 || totem >= g_MobaTotemCount || !IsValidTotemColor(g_MobaTotems[totem][2]))
        return;

    float mx, my;
    if (!frame.TileToRef((float)g_MobaTotems[totem][0], (float)g_MobaTotems[totem][1], mx, my))
        return;
    const float half = TOTEM_SIZE * frame.markers / 2.f;
    const int y = (int)(my - TOOLTIP_HALF_HEIGHT);
    const wchar_t* name = TOTEM_NAMES[g_MobaTotems[totem][2]];

    const DWORD backColor = g_pRenderText->GetTextColor();
    const DWORD backBg = g_pRenderText->GetBgColor();
    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetTextColor(255, 255, 255, 255);
    g_pRenderText->SetBgColor(0, 0, 0, TOOLTIP_BG_ALPHA);
    // Towards the screen centre, so the name never runs off the edge the map is pinned to.
    if (mx > REFERENCE_WIDTH / 2.f)
        g_pRenderText->RenderText((int)(mx - half - TOOLTIP_GAP), y, name, 0, 0, RT3_WRITE_RIGHT_TO_LEFT);
    else
        g_pRenderText->RenderText((int)(mx + half + TOOLTIP_GAP), y, name, 0, 0, RT3_SORT_LEFT);
    g_pRenderText->SetTextColor(backColor);
    g_pRenderText->SetBgColor(backBg);
}

void RenderFountains(const Frame& frame, const Atlases& atlases)
{
    if (!atlases.iconsLoaded)
    {
        RenderFountainsFlat(frame);
        return;
    }

    EnableAlphaBlend3();
    const AtlasGrid icons = IconGrid(atlases);
    const float w = FOUNTAIN_SIZE * frame.markers;
    for (int k = 0; k < MOBA_FOUNTAIN_COUNT; ++k)
    {
        const MobaFountainInfo& info = g_MobaFountainInfo[k];
        float mx, my;
        if (!frame.TileToRef((float)g_MobaFountains[k][0], (float)g_MobaFountains[k][1], mx, my))
            continue;
        if (IsTeam(info.owner))
            DrawHealZone(frame, icons, mx, my, info.owner);
        SetColor(COLOR_WHITE);
        DrawCell(frame, icons, FountainCell(info.owner), mx, my, w);
        DrawCaptureRing(frame, icons, info, mx, my, w);
    }
    SetColor(COLOR_WHITE);
}

void RenderCameraView(const Frame& frame)
{
    if (Hero == nullptr)
        return;

    // The four corners of the screen, cast onto the ground at the height of the hero.
    const int corners[4][2] = { { 0, 0 }, { REFERENCE_WIDTH, 0 }, { REFERENCE_WIDTH, REFERENCE_HEIGHT }, { 0, REFERENCE_HEIGHT } };
    const float groundZ = Hero->Object.Position[2];
    float tileX[4];
    float tileY[4];
    for (int i = 0; i < 4; ++i)
    {
        vec3_t target;
        CameraProjection::ScreenToWorldRay(g_Camera, corners[i][0], corners[i][1], target, false);
        const float originX = MousePosition[0];
        const float originY = MousePosition[1];
        const float originZ = MousePosition[2];
        const float dx = target[0] - originX;
        const float dy = target[1] - originY;
        const float dz = target[2] - originZ;
        float t = 1.f;   // the far end of the ray when it does not reach the ground
        if (fabsf(dz) > 0.001f)
        {
            const float hit = (groundZ - originZ) / dz;
            if (hit > 0.f && hit < 1.f)
                t = hit;
        }
        tileX[i] = (originX + dx * t) / TERRAIN_SCALE;
        tileY[i] = (originY + dy * t) / TERRAIN_SCALE;
    }

    // The ground the camera shows is a trapezoid, turned like the minimap art. The user wants an upright rectangle (not diagonal),
    // bigger: the box that holds the four corners on the minimap, grown by VIEW_BOX_GROWTH.
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (int i = 0; i < 4; ++i)
    {
        float mx, my;
        frame.TileToRef(tileX[i], tileY[i], mx, my);
        minX = std::min(minX, mx);
        maxX = std::max(maxX, mx);
        minY = std::min(minY, my);
        maxY = std::max(maxY, my);
    }

    constexpr float VIEW_BOX_GROWTH = 1.35f;
    constexpr float VIEW_BOX_MIN_SIZE = 18.f;   // reference px
    const float centreX = 0.5f * (minX + maxX);
    const float centreY = 0.5f * (minY + maxY);
    // A landscape rectangle like the screen it stands for (16:9): at least VIEW_BOX_ASPECT times as wide as it is tall, stretched sideways.
    constexpr float VIEW_BOX_ASPECT = 1.8f;
    const float halfH = std::max(0.5f * (maxY - minY) * VIEW_BOX_GROWTH, VIEW_BOX_MIN_SIZE * 0.5f);
    const float halfWide = std::max(0.5f * (maxX - minX) * VIEW_BOX_GROWTH, halfH * VIEW_BOX_ASPECT);
    // The user found it too big (2026-10-18): 40 % shorter and 10 % less tall.
    constexpr float VIEW_BOX_LENGTH_SCALE = 0.60f;
    constexpr float VIEW_BOX_HEIGHT_SCALE = 0.90f;
    const float halfW = halfWide * VIEW_BOX_LENGTH_SCALE;
    const float halfTall = halfH * VIEW_BOX_HEIGHT_SCALE;
    const float left = centreX - halfW, right = centreX + halfW, top = centreY - halfTall, bottom = centreY + halfTall;

    EnableAlphaBlend3();
    SetColor(COLOR_WHITE, 0.95f);
    const float thick = 1.6f;                                  // reference px
    const float thickY = thick * frame.sx / frame.sy;          // same thickness on screen vertically
    // Only the parts of the outline that lie inside the minimap square are drawn.
    const float limitX = 0.5f * frame.sidePx / frame.sx;
    const float limitY = 0.5f * frame.sidePx / frame.sy;
    auto inside = [&](float px, float py)
    {
        return fabsf(px - frame.centreX) <= limitX && fabsf(py - frame.centreY) <= limitY;
    };
    auto horizontal = [&](float y)
    {
        for (float x = left; x <= right; x += 1.f)
        {
            if (inside(x, y))
                RenderColor(x, y - thickY / 2.f, std::min(1.2f, right - x + 0.2f), thickY);
        }
    };
    auto vertical = [&](float x)
    {
        for (float y = top; y <= bottom; y += 1.f * frame.sx / frame.sy)
        {
            if (inside(x, y))
                RenderColor(x - thick / 2.f, y, thick, std::min(1.2f * frame.sx / frame.sy, bottom - y + 0.2f));
        }
    };
    horizontal(top);
    horizontal(bottom);
    vertical(left);
    vertical(right);
    SetColor(COLOR_WHITE);
}

void RenderCreeps(const Frame& frame, const Atlases& atlases)
{
    constexpr float CREEP_DOT_SIZE = 2.6f;   // reference px, before the marker size of the options panel
    constexpr float CREEP_FLAT_SIZE = 1.6f;
    const std::vector<World::Moba::MinimapCreeps::Dot>& dots = World::Moba::MinimapCreeps::Get();
    if (dots.empty())
        return;

    if (atlases.portraitsLoaded)
        EnableAlphaBlend3();

    const AtlasGrid portraits = PortraitGrid(atlases);
    for (const World::Moba::MinimapCreeps::Dot& dot : dots)
    {
        if (!IsTeam(dot.team))
            continue;

        float mx, my;
        if (!frame.TileToRef((float)dot.x, (float)dot.y, mx, my))
            continue;

        SetColor(TeamColor(dot.team));
        if (atlases.portraitsLoaded)
            DrawCell(frame, portraits, PORTRAIT_DISC, mx, my, CREEP_DOT_SIZE * frame.markers);
        else
            FlatSquare(frame, mx, my, CREEP_FLAT_SIZE * frame.markers);
    }

    if (!atlases.portraitsLoaded)
        EndRenderColor();
    SetColor(COLOR_WHITE);
}

void RenderFeed(const Frame& frame, const Atlases& atlases)
{
    RenderFeedStructures(frame, atlases);
    RenderFeedChampions(frame, atlases);
    SetColor(COLOR_WHITE);
}

bool RenderOwnHero(const Frame& frame, const Atlases& atlases)
{
    if (!atlases.portraitsLoaded || Hero == nullptr)
        return false;

    EnableAlphaBlend3();
    float mx, my;
    frame.TileToRef((float)Hero->PositionX, (float)Hero->PositionY, mx, my);
    const float w = HERO_SIZE * frame.markers;
    const bool dead = Hero->Dead > 0.f;
    const int portrait = PortraitIndex(gCharacterManager.GetBaseClass(Hero->Class));
    DrawChampionToken(frame, atlases, mx, my, w, portrait, dead, dead ? COLOR_DEAD : COLOR_HERO_GOLD);
    DrawOwnRecallProgress(frame, atlases, mx, my, w);
    SetColor(COLOR_WHITE);
    return true;
}

void SetEntryColor(const MobaMinimapEntry& e)
{
    if (e.kind == KIND_WARD)
        SetColor(COLOR_WARD);
    else if (e.kind == KIND_CHAMPION && e.hp == 0)
        SetColor(COLOR_DEAD);
    else
        SetColor(TeamColor(e.team));
}

float EntrySize(const MobaMinimapEntry& e, float champion, float structure, float nexus, float ward)
{
    switch (e.kind)
    {
    case KIND_CHAMPION: return champion;
    case KIND_TURRET:   return structure;
    case KIND_NEXUS:    return nexus;
    default:            return ward;
    }
}

}
