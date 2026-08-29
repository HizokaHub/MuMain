// MobaCamera.cpp - MOBA-style detached edge-pan camera

#include "stdafx.h"
#include "MobaCamera.h"
#include "CameraManager.h"
#include "Core/Input/KeyState.h"
#include "Core/Math/ZzzMathLib.h"
#include "Engine/Object/ZzzCharacter.h"
#include <algorithm>
#include <cmath>

// Mouse cursor position, already normalised to the 640x480 reference space
// (see Winmain.cpp: MouseX = winX / g_fScreenRate_x, clamped to the edges).
extern int MouseX;
extern int MouseY;

// Mouse wheel delta, normalised to ticks in Winmain.
extern int MouseWheel;

// Millisecond frame clock (Render/Textures/ZzzOpenglUtil.h).
extern double WorldTime;

// The local player character.
extern CHARACTER* Hero;

MobaCamera::MobaCamera(CameraState& state)
    : m_State(state)
    , m_pDefaultCamera(std::make_unique<DefaultCamera>(state))
    , m_Config(CameraConfig::ForMainSceneDefaultCamera())
    , m_FocusInitialized(false)
    , m_ZoomScale(ZOOM_DEFAULT)
    , m_LastWorldTime(0.0)
{
    IdentityVector3D(m_FocusWorld);
    BuildWideConfig();
}

void MobaCamera::BuildWideConfig()
{
    m_Config = CameraConfig::ForMainSceneDefaultCamera();
    m_Config.farPlane *= VIEW_RANGE_SCALE;
    m_Config.objectCullRange *= VIEW_RANGE_SCALE;
    m_Config.terrainCullRange *= VIEW_RANGE_SCALE;
    m_Config.fogStart = m_Config.farPlane;
    m_Config.fogEnd = m_Config.farPlane * 1.25f;
}

void MobaCamera::OnActivate(const CameraState& previousState)
{
    // Anchor the focus on the Hero so the first frame renders exactly like the
    // Default camera and the F9 switch is seamless.
    m_FocusInitialized = false;
    m_ZoomScale = ZOOM_DEFAULT;
    m_LastWorldTime = WorldTime;
    m_pDefaultCamera->OnActivate(previousState);
    AnchorFocusToHero();
}

void MobaCamera::OnDeactivate()
{
    m_pDefaultCamera->OnDeactivate();
}

void MobaCamera::Reset()
{
    m_FocusInitialized = false;
    m_ZoomScale = ZOOM_DEFAULT;
    m_pDefaultCamera->Reset();
}

void MobaCamera::ResetView()
{
    // F11: recenter on the Hero and drop back to the default zoom.
    AnchorFocusToHero();
    m_ZoomScale = ZOOM_DEFAULT;
    m_pDefaultCamera->ResetView();
}

void MobaCamera::GetFocusOffset(vec3_t out) const
{
    vec3_t heroPos;
    if (!m_FocusInitialized || !GetHeroGroundPos(heroPos))
    {
        IdentityVector3D(out);
        return;
    }

    out[0] = m_FocusWorld[0] - heroPos[0];
    out[1] = m_FocusWorld[1] - heroPos[1];
    out[2] = 0.0f;
}

bool MobaCamera::Update()
{
    const double frameSeconds = ConsumeFrameSeconds();

    // Read the wheel before DefaultCamera::Update, which would otherwise
    // consume it into its own (short) zoom ladder.
    HandleWheelZoom();

    // DefaultCamera positions the rig on the Hero and handles mount lift / FOV,
    // writing straight into m_State (== g_Camera).
    m_pDefaultCamera->Update();

    vec3_t heroPos;
    if (!GetHeroGroundPos(heroPos))
        return false;  // no Hero yet - behave as the plain Default camera

    if (!m_FocusInitialized)
        AnchorFocusToHero();

    UpdateFocusWorld(frameSeconds, heroPos);
    ClampFocusToHero(heroPos);
    ApplyFocusToState(heroPos);

    return false;  // never locks input
}

double MobaCamera::ConsumeFrameSeconds()
{
    double elapsed = (WorldTime - m_LastWorldTime) / 1000.0;
    m_LastWorldTime = WorldTime;

    if (elapsed < 0.0)
        return 0.0;

    return std::min(elapsed, MAX_FRAME_SECONDS);
}

void MobaCamera::HandleWheelZoom()
{
    if (MouseWheel == 0)
        return;

    // The Moba camera always responds to the wheel — the F10 zoom lock is a
    // Default/Orbital convenience that only gets in the way for a MOBA view.
    const int wheel = MouseWheel;
    MouseWheel = 0;

    // Wheel up (positive ticks) zooms in -> smaller multiplier.
    m_ZoomScale -= wheel * ZOOM_STEP_PER_TICK;
    m_ZoomScale = std::clamp(m_ZoomScale, ZOOM_MIN, ZOOM_MAX);
}

bool MobaCamera::GetHeroGroundPos(vec3_t out) const
{
    if (Hero == nullptr || Hero->Object.Live == 0)
        return false;

    out[0] = Hero->Object.Position[0];
    out[1] = Hero->Object.Position[1];
    out[2] = 0.0f;
    return true;
}

void MobaCamera::AnchorFocusToHero()
{
    vec3_t heroPos;
    if (!GetHeroGroundPos(heroPos))
        return;

    VectorCopy(heroPos, m_FocusWorld);
    m_FocusInitialized = true;
}

void MobaCamera::UpdateFocusWorld(double frameSeconds, const vec3_t heroPos)
{
    // Recenter key: snap straight onto the Hero and stay pinned while held.
    // One tap fully centres the view; holding it keeps following the Hero.
    if (Core::Input::IsKeyDown(RECENTER_KEY))
    {
        VectorCopy(heroPos, m_FocusWorld);
        return;
    }

    EdgePanFocus(frameSeconds, heroPos);
}

void MobaCamera::EdgePanFocus(double frameSeconds, const vec3_t /*heroPos*/)
{
    float horizontal = 0.0f;
    if (MouseX <= EDGE_MARGIN_REF_PX)
        horizontal = -1.0f;
    else if (MouseX >= REFERENCE_WIDTH - EDGE_MARGIN_REF_PX)
        horizontal = 1.0f;

    float forward = 0.0f;
    if (MouseY <= EDGE_MARGIN_REF_PX)
        forward = 1.0f;          // cursor at the top edge -> pan away from viewer
    else if (MouseY >= REFERENCE_HEIGHT - BOTTOM_EDGE_REF_PX)
        forward = -1.0f;         // cursor jammed against the very bottom -> pan toward viewer

    if (horizontal == 0.0f && forward == 0.0f)
        return;

    vec3_t right;
    vec3_t fwd;
    GroundPanAxes(right, fwd);

    // Faster pan the further out you are zoomed, so a screen's worth of travel
    // takes about the same time at any zoom.
    const float distanceScale = (m_State.Distance * m_ZoomScale) / PAN_REFERENCE_DISTANCE;
    const float step = PAN_SPEED_UNITS_PER_SEC * std::max(distanceScale, 0.25f) * static_cast<float>(frameSeconds);

    m_FocusWorld[0] += (right[0] * horizontal + fwd[0] * forward) * step;
    m_FocusWorld[1] += (right[1] * horizontal + fwd[1] * forward) * step;
}

void MobaCamera::GroundPanAxes(vec3_t outRight, vec3_t outForward) const
{
    float matrix[3][4];
    AngleMatrix(m_State.Angle, matrix);

    // Row 0 = right, row 1 = forward — same convention OrbitalCamera reads from
    // AngleMatrix. Flattened onto the ground plane so panning never changes Z.
    outRight[0] = matrix[0][0];
    outRight[1] = matrix[0][1];
    outRight[2] = 0.0f;

    outForward[0] = matrix[1][0];
    outForward[1] = matrix[1][1];
    outForward[2] = 0.0f;

    VectorNormalize(outRight);
    VectorNormalize(outForward);
}

void MobaCamera::ClampFocusToHero(const vec3_t heroPos)
{
    const float dx = m_FocusWorld[0] - heroPos[0];
    const float dy = m_FocusWorld[1] - heroPos[1];
    const float lengthSq = dx * dx + dy * dy;
    if (lengthSq <= MAX_FOCUS_RADIUS * MAX_FOCUS_RADIUS)
        return;

    const float scale = MAX_FOCUS_RADIUS / std::sqrt(lengthSq);
    m_FocusWorld[0] = heroPos[0] + dx * scale;
    m_FocusWorld[1] = heroPos[1] + dy * scale;
}

void MobaCamera::ApplyFocusToState(const vec3_t heroPos)
{
    // 1. Shift the rig so it looks at m_FocusWorld instead of the Hero. Because
    //    the shift is (focus - heroPos), Hero movement alone doesn't move the
    //    camera: as the Hero walks toward the focus the shift shrinks.
    m_State.Position[0] += m_FocusWorld[0] - heroPos[0];
    m_State.Position[1] += m_FocusWorld[1] - heroPos[1];

    // 2. Zoom: scale the camera's offset from the focus ground point by
    //    m_ZoomScale, keeping the isometric angle. The Hero's real Z is the
    //    ground reference (focus terrain height is assumed similar).
    const float pivotZ = (Hero != nullptr) ? Hero->Object.Position[2] : 0.0f;
    const vec3_t pivot = { m_FocusWorld[0], m_FocusWorld[1], pivotZ };
    m_State.Position[0] = pivot[0] + (m_State.Position[0] - pivot[0]) * m_ZoomScale;
    m_State.Position[1] = pivot[1] + (m_State.Position[1] - pivot[1]) * m_ZoomScale;
    m_State.Position[2] = pivot[2] + (m_State.Position[2] - pivot[2]) * m_ZoomScale;

    m_State.UpdateMatrix();

    // 3. Apply the widened far / cull ranges and rebuild the frustum at the
    //    zoomed-out position (SetConfig with an equal config is the codebase's
    //    frustum-rebuild trick). DefaultCamera already wrote Angle/FOV.
    m_pDefaultCamera->SetConfig(m_Config);
    m_State.ViewFar = m_Config.farPlane;
}
