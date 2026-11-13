#pragma once

// MOBA stress meter: measures how much the client can take when the arena is full of creatures (the 3v3 commanders mode launches whole waves).
// Every frame it samples the wall time, the update time and the render time; every 5 seconds it closes a window and, if at least MIN_CHARACTERS
// characters were alive in it, writes one "[MOBA-STRESS]" line to MuError.log (fps, average / p95 / p99 / max frame time, the "work" time without
// the vsync wait, the live / drawn characters and the time of each pass of the FrameProfiler) and refreshes the small HUD line.
// It is off by default; "$stress on" / "$stress off" in the console switch it. With fewer characters than MIN_CHARACTERS it writes and draws nothing.
// A frame that lasts over one second (a pause, or a hitch the test is looking for) is not part of the averages: it is counted as "stalls" with
// the longest one in "stallMax" (ms) on the log line.
// The log line is only read by the developer, so it is not translated; the HUD line (drawn by SceneManager.cpp) is in Spanish.
namespace World::Moba::StressMeter
{
    constexpr int MIN_CHARACTERS = 60;      // below this many live characters the meter stays silent
    constexpr double WINDOW_MS = 5000.0;    // length of one measuring window

    // What the HUD line shows: the values of the last closed window.
    struct OverlayInfo
    {
        bool visible = false;
        double fps = 0.0;
        double workAvgMs = 0.0;     // update + render without the present / vsync wait, average
        double workP95Ms = 0.0;     // same, 95th percentile
        int units = 0;              // live characters in the last frame of the window
    };

    void SetEnabled(bool enabled);
    bool IsEnabled();

    // First line of RenderScene: stamps the time since the previous frame.
    void BeginFrame();
    // Right after UpdateSceneState() in RenderScene.
    void MarkUpdateDone();
    // In MainScene, right before FrameProfiler::ResetFrame() (that call zeroes the pass accumulators read here).
    void CapturePasses();
    // After the scene switch of RenderScene: closes the sample, and the window when it is time.
    void EndFrame();

    const OverlayInfo& Overlay();
}
