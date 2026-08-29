#pragma once

#include "ICamera.h"
#include "CameraState.h"
#include "CameraConfig.h"
#include "DefaultCamera.h"
#include <memory>

/**
 * @brief MOBA-style detached pan camera.
 *
 * Reuses the DefaultCamera pose (same top-down angle, zoom ladder, mount lift
 * and FOV) but points the camera at a fixed WORLD point instead of the Hero:
 *
 * - The focus is a world position, not an offset from the Hero, so once the
 *   view is panned it stays put while the character walks around underneath it.
 * - Moving the mouse cursor to a screen edge scrolls the focus that way
 *   (League-of-Legends style edge panning). Pan speed scales with the current
 *   zoom so it feels consistent whether zoomed in or out.
 * - The mouse wheel zooms in/out around the focus point over a much wider
 *   range than the Default camera's rungs (subject to the F10 zoom lock).
 * - Tapping the recenter key (see RECENTER_KEY) snaps the focus onto the Hero;
 *   holding it keeps the focus pinned to the Hero (follow).
 * - F11 (ResetView) snaps the focus back to the Hero and resets the zoom.
 *
 * The camera is produced by translating the DefaultCamera's computed position
 * on the ground (XY) plane by (focus - heroPos). Culling / frustum / config
 * queries delegate to the internal DefaultCamera, re-evaluated at the panned
 * position each frame.
 *
 * MainScene only (the CameraManager blocks activation elsewhere).
 */
class MobaCamera : public ICamera
{
public:
    explicit MobaCamera(CameraState& state);
    ~MobaCamera() override = default;

    // ICamera interface
    bool Update() override;
    void Reset() override;
    void OnActivate(const CameraState& previousState) override;
    void OnDeactivate() override;
    void ResetView() override;
    const char* GetName() const override { return "Moba"; }

    // Config / frustum / culling all mirror the internal DefaultCamera,
    // rebuilt at the panned position in Update().
    const CameraConfig& GetConfig() const override { return m_pDefaultCamera->GetConfig(); }
    void SetConfig(const CameraConfig& config) override { m_pDefaultCamera->SetConfig(config); }
    const Frustum& GetFrustum() const override { return m_pDefaultCamera->GetFrustum(); }

    bool ShouldCullObject(const vec3_t position, float radius) const override
    {
        return m_pDefaultCamera->ShouldCullObject(position, radius);
    }

    bool ShouldCullTerrain(int tileX, int tileY) const override
    {
        return m_pDefaultCamera->ShouldCullTerrain(tileX, tileY);
    }

    bool ShouldCullObject2D(float x, float y, float radius) const override
    {
        return m_pDefaultCamera->ShouldCullObject2D(x, y, radius);
    }

    // Current focus - hero offset on the ground plane, for debug / UI overlays.
    void GetFocusOffset(vec3_t out) const;

private:
    // Recenter binding. 'Y' keeps clear of the hotkey bar (Space) and movement
    // keys. Tap = snap focus onto the Hero; hold = keep it pinned (follow).
    static constexpr int RECENTER_KEY = 'Y';

    // Screen-edge band (in the 640x480 reference space MouseX/MouseY live in).
    static constexpr int EDGE_MARGIN_REF_PX = 12;
    // The bottom of the screen is covered full-width by the HUD bar, so the
    // downward-pan band is a thin strip at the very bottom edge only -- below
    // the bar's clickable area, so clicking HUD buttons never pans the view.
    static constexpr int BOTTOM_EDGE_REF_PX = 3;

    // Pan speed in world units per second at the reference zoom distance,
    // scaled linearly by (current distance / PAN_REFERENCE_DISTANCE).
    static constexpr float PAN_SPEED_UNITS_PER_SEC = 1500.0f;
    static constexpr float PAN_REFERENCE_DISTANCE = 1300.0f;

    // Wheel zoom: a multiplier on the Default-camera distance from the focus.
    // 1.0 == the Default camera's look; the top of the range is a comfortable
    // MOBA overview, deliberately not a huge pull-back. F11 resets to ZOOM_DEFAULT.
    static constexpr float ZOOM_DEFAULT = 1.0f;
    static constexpr float ZOOM_MIN = 0.7f;
    static constexpr float ZOOM_MAX = 1.8f;
    static constexpr float ZOOM_STEP_PER_TICK = 0.12f;

    // Far plane / cull ranges are widened by this factor (relative to the
    // Default camera's) so terrain and objects don't get clipped when zoomed
    // out. Sized for ZOOM_MAX with headroom.
    static constexpr float VIEW_RANGE_SCALE = 2.0f;

    // How far the focus may stray from the Hero until an explicit arena
    // boundary exists. Roughly 200 tiles (world units ~= tile * 100), enough
    // to sweep across most of a full 256-tile map from the centre while still
    // keeping the camera bounded. A per-arena boundary will replace this.
    static constexpr float MAX_FOCUS_RADIUS = 20000.0f;

    // Frame-time clamp so a long stall (loading screen) can't teleport the pan.
    static constexpr double MAX_FRAME_SECONDS = 0.1;

    CameraState& m_State;
    std::unique_ptr<DefaultCamera> m_pDefaultCamera;  // computes the base pose
    CameraConfig m_Config;    // widened far/cull ranges for zoomed-out views

    vec3_t m_FocusWorld;       // world point the camera looks at (Z ignored)
    bool   m_FocusInitialized; // false until the first frame anchors it to Hero
    float  m_ZoomScale;        // wheel zoom multiplier, ZOOM_MIN..ZOOM_MAX
    double m_LastWorldTime;    // for frame-time-independent panning

    void BuildWideConfig();
    double ConsumeFrameSeconds();
    bool GetHeroGroundPos(vec3_t out) const;
    void AnchorFocusToHero();
    void HandleWheelZoom();
    void UpdateFocusWorld(double frameSeconds, const vec3_t heroPos);
    void EdgePanFocus(double frameSeconds, const vec3_t heroPos);
    void ClampFocusToHero(const vec3_t heroPos);
    void ApplyFocusToState(const vec3_t heroPos);
    void GroundPanAxes(vec3_t outRight, vec3_t outForward) const;
};
