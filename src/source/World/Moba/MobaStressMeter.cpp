#include "stdafx.h"
#include "World/Moba/MobaStressMeter.h"

#include "Core/Utilities/FrameProfiler.h"
#include "Engine/Object/ZzzCharacter.h"        // g_LastLiveCharacterCount, g_LastRenderedCharacterCount, g_CharacterSlotOverflowCount
#include "Render/Textures/ZzzOpenglUtil.h"     // IsVSyncEnabled
#include "Scenes/SceneManager.h"               // GetTargetFps

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>

extern EGameScene SceneFlag;
extern bool g_bDisableAnimationTaskPool;       // Winmain.cpp: the animation thread pool is off unless config.ini asks for it
extern bool g_LastAnimationWasParallel;        // ZzzCharacter.cpp

namespace World::Moba::StressMeter
{

namespace
{
    using Clock = std::chrono::steady_clock;
    using FP = FrameProfiler::Pass;

    constexpr bool DEFAULT_ENABLED = false;         // real matches must not log or draw anything: "$stress on" starts the test
    constexpr int MAX_SAMPLES = 4096;               // frames kept for the percentiles (the sums and the max stay exact beyond it)
    constexpr double MAX_WALL_MS = 1000.0;          // a longer gap is a pause (inactive window, loading), not a frame
    constexpr double PERCENTILE_95 = 0.95;
    constexpr double PERCENTILE_99 = 0.99;
    constexpr double MS_PER_SECOND = 1000.0;

    // The FrameProfiler passes that the log line breaks the frame into.
    enum TrackedPass
    {
        TP_MOVE_CHARS,
        TP_CHARACTERS,
        TP_CHAR_WAIT,
        TP_SKINNING,
        TP_MOVE_EFFECTS,
        TP_MOVE_PARTICLES,
        TP_EFFECTS,
        TP_SPRITES,
        TP_PARTICLES,
        TP_JOINTS,
        TP_COUNT
    };
    constexpr FP TRACKED[TP_COUNT] = {
        FP::MoveChars, FP::Characters, FP::CharWait, FP::Skinning, FP::MoveEffects,
        FP::MoveParticles, FP::Effects, FP::Sprites, FP::Particles, FP::Joints
    };

    bool s_enabled = DEFAULT_ENABLED;
    OverlayInfo s_overlay;

    // Frame in progress.
    bool s_frameOpen = false;
    bool s_havePrevBegin = false;
    Clock::time_point s_prevBegin;
    Clock::time_point s_frameStart;
    Clock::time_point s_updateDone;
    double s_wallMs = 0.0;                          // time since the previous BeginFrame
    bool s_wallValid = false;
    double s_stallMs = 0.0;                         // gap of this frame when it exceeded MAX_WALL_MS, else 0

    // Measuring window.
    bool s_windowStarted = false;
    Clock::time_point s_windowStart;
    int s_frames = 0;
    int s_sampleCount = 0;
    int s_passFrames = 0;
    float s_wallSamples[MAX_SAMPLES];
    float s_workSamples[MAX_SAMPLES];
    double s_wallSum = 0.0, s_wallMax = 0.0;
    double s_workSum = 0.0;
    double s_updateSum = 0.0, s_renderSum = 0.0, s_presentSum = 0.0;
    double s_passSum[TP_COUNT];
    int s_stalls = 0;                               // frames of the window that were left out of the stats for lasting over MAX_WALL_MS
    double s_stallMax = 0.0;
    int s_peakAlive = 0;
    int s_lastAlive = 0;
    int s_lastDrawn = 0;

    double MsBetween(Clock::time_point a, Clock::time_point b)
    {
        return std::chrono::duration<double, std::milli>(b - a).count();
    }

    void ResetWindow()
    {
        s_windowStarted = false;
        s_frames = 0;
        s_sampleCount = 0;
        s_passFrames = 0;
        s_wallSum = s_wallMax = s_workSum = 0.0;
        s_stalls = 0;
        s_stallMax = 0.0;
        s_updateSum = s_renderSum = s_presentSum = 0.0;
        std::fill(std::begin(s_passSum), std::end(s_passSum), 0.0);
        s_peakAlive = s_lastAlive = s_lastDrawn = 0;
    }

    // Value at the given percentile. Reorders the samples; they are thrown away with the window anyway.
    double Percentile(float* samples, int count, double fraction)
    {
        if (count <= 0)
            return 0.0;
        const int rank = std::clamp(static_cast<int>(std::ceil(fraction * count)) - 1, 0, count - 1);
        std::nth_element(samples, samples + rank, samples + count);
        return samples[rank];
    }

    double PassAvg(TrackedPass pass)
    {
        return (s_passFrames > 0) ? s_passSum[pass] / s_passFrames : 0.0;
    }

    void WriteLog(double windowSec, double wallP95, double wallP99, double workP95)
    {
        const double frames = s_frames;
        const double fps = (windowSec > 0.0) ? frames / windowSec : 0.0;
        g_ErrorReport.Write(
            L"[MOBA-STRESS] window=%.1fs frames=%d fps=%.1f ms avg=%.2f p95=%.2f p99=%.2f max=%.2f stalls=%d stallMax=%.0f"
            L" | work avg=%.2f p95=%.2f | upd=%.2f rend=%.2f pres=%.2f"
            L" | chars alive=%d peak=%d drawn=%d overflow=%d"
            L" | moveChars=%.2f charsPass=%.2f wait=%.2f skin=%.2f"
            L" | moveFx=%.2f moveParts=%.2f effects=%.2f sprites=%.2f particles=%.2f joints=%.2f"
            L" | pool=%d parallel=%d vsync=%d fpsCap=%.0f\r\n",
            windowSec, s_frames, fps, s_wallSum / frames, wallP95, wallP99, s_wallMax, s_stalls, s_stallMax,
            s_workSum / frames, workP95, s_updateSum / frames, s_renderSum / frames, s_presentSum / frames,
            s_lastAlive, s_peakAlive, s_lastDrawn, g_CharacterSlotOverflowCount,
            PassAvg(TP_MOVE_CHARS), PassAvg(TP_CHARACTERS), PassAvg(TP_CHAR_WAIT), PassAvg(TP_SKINNING),
            PassAvg(TP_MOVE_EFFECTS), PassAvg(TP_MOVE_PARTICLES), PassAvg(TP_EFFECTS),
            PassAvg(TP_SPRITES), PassAvg(TP_PARTICLES), PassAvg(TP_JOINTS),
            g_bDisableAnimationTaskPool ? 0 : 1, g_LastAnimationWasParallel ? 1 : 0,
            IsVSyncEnabled() ? 1 : 0, GetTargetFps());
    }

    // Turns the window into the log line and the HUD values. Only called with frames > 0.
    void Report(Clock::time_point now)
    {
        const double windowSec = MsBetween(s_windowStart, now) / MS_PER_SECOND;
        const int count = s_sampleCount;
        const double wallP95 = Percentile(s_wallSamples, count, PERCENTILE_95);
        const double wallP99 = Percentile(s_wallSamples, count, PERCENTILE_99);
        const double workP95 = Percentile(s_workSamples, count, PERCENTILE_95);

        WriteLog(windowSec, wallP95, wallP99, workP95);

        s_overlay.visible = true;
        s_overlay.fps = (windowSec > 0.0) ? s_frames / windowSec : 0.0;
        s_overlay.workAvgMs = s_workSum / s_frames;
        s_overlay.workP95Ms = workP95;
        s_overlay.units = s_lastAlive;
    }

    void CloseWindow(Clock::time_point now)
    {
        const bool busy = s_frames > 0 && s_peakAlive >= MIN_CHARACTERS;
        if (busy)
            Report(now);
        else
            s_overlay.visible = false;

        ResetWindow();
    }

    void RecordSample(Clock::time_point now)
    {
        const double updateMs = MsBetween(s_frameStart, s_updateDone);
        const double sceneMs = MsBetween(s_updateDone, now);
        const double presentMs = FrameProfiler::AccumulatorMs(FP::Present);
        const double renderMs = std::max(0.0, sceneMs - presentMs);
        const double workMs = updateMs + renderMs;

        if (s_sampleCount < MAX_SAMPLES)
        {
            s_wallSamples[s_sampleCount] = static_cast<float>(s_wallMs);
            s_workSamples[s_sampleCount] = static_cast<float>(workMs);
            ++s_sampleCount;
        }
        ++s_frames;
        s_wallSum += s_wallMs;
        s_wallMax = std::max(s_wallMax, s_wallMs);
        s_workSum += workMs;
        s_updateSum += updateMs;
        s_renderSum += renderMs;
        s_presentSum += presentMs;

        s_lastAlive = g_LastLiveCharacterCount;
        s_lastDrawn = g_LastRenderedCharacterCount;
        s_peakAlive = std::max(s_peakAlive, s_lastAlive);
    }
}

void SetEnabled(bool enabled)
{
    s_enabled = enabled;
    s_frameOpen = false;
    s_havePrevBegin = false;
    s_overlay.visible = false;
    ResetWindow();
}

bool IsEnabled()
{
    return s_enabled;
}

void BeginFrame()
{
    if (!s_enabled)
        return;

    s_frameStart = Clock::now();
    s_updateDone = s_frameStart;
    s_wallValid = false;
    s_stallMs = 0.0;
    if (s_havePrevBegin)
    {
        s_wallMs = MsBetween(s_prevBegin, s_frameStart);
        s_wallValid = s_wallMs <= MAX_WALL_MS;
        if (!s_wallValid)
            s_stallMs = s_wallMs;
    }
    s_prevBegin = s_frameStart;
    s_havePrevBegin = true;
    s_frameOpen = true;
}

void MarkUpdateDone()
{
    if (!s_enabled || !s_frameOpen)
        return;

    s_updateDone = Clock::now();
}

void CapturePasses()
{
    // Same frames as RecordSample (valid wall time only), so the per-pass averages share the denominator of work / upd / rend.
    if (!s_enabled || !s_frameOpen || !s_wallValid || SceneFlag != MAIN_SCENE)
        return;

    for (int i = 0; i < TP_COUNT; ++i)
        s_passSum[i] += FrameProfiler::AccumulatorMs(TRACKED[i]);
    ++s_passFrames;
}

void EndFrame()
{
    if (!s_enabled || !s_frameOpen)
        return;
    s_frameOpen = false;

    if (SceneFlag != MAIN_SCENE)
    {
        s_overlay.visible = false;
        s_havePrevBegin = false;                    // the first frame back in the arena must not measure the loading gap
        ResetWindow();
        return;
    }

    const Clock::time_point now = Clock::now();
    if (!s_windowStarted)
    {
        s_windowStarted = true;
        s_windowStart = s_frameStart;
    }

    if (s_stallMs > 0.0)
    {
        ++s_stalls;
        s_stallMax = std::max(s_stallMax, s_stallMs);
    }

    if (s_wallValid)
        RecordSample(now);

    if (MsBetween(s_windowStart, now) >= WINDOW_MS)
        CloseWindow(now);
}

const OverlayInfo& Overlay()
{
    return s_overlay;
}

}
