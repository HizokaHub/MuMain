# Camera System

User guide for the in-game cameras. Covers what cameras you can switch
between, the keys to control them, and the player-visible changes that
landed in this rework.

For the in-editor tuning UI, see [`dev-editor.md`](dev-editor.md).

---

## Cameras

| Camera | Where | Notes |
|--------|-------|-------|
| **Default** | Everywhere | Original third-person follow. |
| **Orbital** | Gameplay maps only | Middle-mouse drag to look around, wheel to zoom. |
| **Moba** | Gameplay maps only | Detached edge-pan view, MOBA style. See below. |
| **FreeFly** | Editor only | Free-look spectator. Not in Release builds. |

Switching cameras outside gameplay (login, character select) is not
supported - only the Default camera runs there. Leaving a gameplay map
automatically returns you to Default.

### Moba camera

Keeps the Default camera's top-down angle, zoom rungs and mount lift, but
lets the view leave the character:

- **Push the mouse cursor against a screen edge** to scroll the view that
  way. Left / right / top use a normal edge band; the bottom is only the
  very last few pixels of the window (below the HUD bar) so clicking HUD
  buttons never scrolls the view.
- Scroll speed scales with the current zoom distance, so it feels the same
  zoomed in or out.
- **Tap `Y`** to snap the view straight back onto your character; **hold `Y`**
  to keep it pinned there (follow). Release to free-pan again.
- **F11** snaps straight back to the character and resets the zoom rung.
- The view is capped at a large fixed distance from the character for now
  (about two thirds of a full map); a per-arena boundary will replace that
  cap later. Terrain and objects are still only drawn within the normal
  render range of wherever the camera is looking, so panning far shows the
  focus area, not a continuous strip back to the character.
- **Mouse wheel** zooms in and out around the focus point, up to a
  comfortable MOBA overview. The wheel is always live in Moba mode (the F10
  zoom lock is a Default/Orbital convenience). F11 resets the zoom.
- **Click-to-move reaches far clicks.** Normally one click walks at most 15
  tiles (the path buffer / move-packet limit), so a click far from the
  character only walks part way. In Moba mode the clicked tile is remembered
  and the character re-paths toward it each time a segment finishes, until it
  arrives. A new click, following an ally, or an attack cancels it. Other
  camera modes keep the one-segment-per-click behaviour.
- **Right-click chases far enemies.** A right-click on an enemy beyond path
  range would otherwise do nothing; in Moba mode the character walks toward
  the (moving) target and attacks once in range. It also works mid-travel:
  right-clicking an enemy while walking to a clicked point drops that order
  and goes after the enemy. Cancelled by a new move/attack order, following
  an ally, death or stun.

---

## Controls

| Key | Action |
|-----|--------|
| **F9** | Cycle to the next camera (Default → Orbital → Moba → Default). |
| **F10** | Toggle zoom lock. Default is **on** so the wheel never zooms by accident. |
| **F11** | Reset the active camera. Default returns to its starting zoom rung; Orbital also resets rotation. |
| **Mouse wheel** | Zoom in / out (when zoom is unlocked). |
| **Middle-mouse drag** | Rotate the Orbital camera. |

Tip: F10 is "global" - toggling it once unlocks the wheel for whichever
camera is active, and it stays unlocked through camera switches until you
press F10 again.

---

## What's new

- **F10 zoom lock and F11 reset.** Both keys work with whichever camera
  is currently active.
- **Orbital wheel zoom remembers your last setting** across sessions (saved
  to `config.ini` under `[Camera] Zoom`).
- **Eight Default zoom rungs** (was five). The ladder now adds three
  closer-in steps below the previous floor; the starting rung sits one
  step in from the original middle. F11 returns you to that starting rung.
- **Per-map camera overrides removed.** Castle Siege, PK Field, the
  6th-character home, and a few others used to forcibly clamp or reposition
  the camera. They now all share the same Default-camera zoom range.
- **Widescreen rendering fix.** On 16:9 the upper-left and upper-right
  screen corners no longer show missing terrain.
- **Editor-only:** the FreeFly cone overlay now draws coloured lines on
  the ground showing where the spectated camera's view actually meets the
  terrain - red at the near edge, yellow at the far edge.

---

## For developers

If you want the architecture, code layout, or tuning sliders, see
[`dev-editor.md`](dev-editor.md) and the merged PRs that introduced this:
[#335](https://github.com/sven-n/MuMain/pull/335) (3D camera rework) and
[#364](https://github.com/sven-n/MuMain/pull/364) (zoom controls).
