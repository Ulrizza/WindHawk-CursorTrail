# WindHawk - Cursor Trail

A fully customizable cursor trail overlay for the Windows desktop.

![Cursor trail](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

## The two styles

- **Simple line** — a polyline that follows the cursor; its width, color, and
  opacity can change from head to tail.
- **Cursor ghost** — faded copies of the cursor image, each latched at the spot
  where it spawned (they stay put and only fade out). Each copy keeps the exact
  cursor image from when it was sampled, so an image change (e.g. arrow to
  I-beam) appears gradually along the trail.

## Features

- **Time based** vs **Size based** trails.
- **Timing** — tail duration and inactivity timeout.
- **Size** — stroke width (Simple line) and copy size (Cursor ghost).
- **Color** — head-to-tail gradients, blend width, interpolation, and ghost
  recoloring.
- **Hotkey** — toggle the trail on/off with a customizable global hotkey.

## Settings

### Style

**`style`** — Rendering style: Simple line or Cursor ghost.

`simple_line`

![style = simple_line](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`cursor_ghost`

![style = cursor_ghost](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

### Simple line options

**`simpleLineOptions.trail_mode`** — `time_based` or `size_based`: how the trail expires.

`time_based`

![simpleLineOptions.trail_mode = time_based](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`size_based`

![simpleLineOptions.trail_mode = size_based](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`simpleLineOptions.timeBased.tail_duration`** — Milliseconds each trail segment stays visible (min 20).

`300`

![simpleLineOptions.timeBased.tail_duration = 300](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`1000`

![simpleLineOptions.timeBased.tail_duration = 1000](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`2000`

![simpleLineOptions.timeBased.tail_duration = 2000](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`simpleLineOptions.sizeBased.tail_size`** — Total trail length in pixels (min 20).

`500`

![simpleLineOptions.sizeBased.tail_size = 500](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`1500`

![simpleLineOptions.sizeBased.tail_size = 1500](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`3000`

![simpleLineOptions.sizeBased.tail_size = 3000](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`simpleLineOptions.sizeBased.timeout`** — Milliseconds of inactivity before the trail fades, using the Time based duration (0 = disabled).

`0`

![simpleLineOptions.sizeBased.timeout = 0](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`1000`

![simpleLineOptions.sizeBased.timeout = 1000](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`5000`

![simpleLineOptions.sizeBased.timeout = 5000](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`simpleLineOptions.width.values`** — Comma-separated stroke widths from head to tail. Each value gets an equal share; repeat to widen (e.g. `2,2,2,2,1`).

`3`

![simpleLineOptions.width.values = 3](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`5,1`

![simpleLineOptions.width.values = 5,1](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`10,1,10,1`

![simpleLineOptions.width.values = 10,1,10,1](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`simpleLineOptions.color.values`** — Hex color(s) (`RRGGBB`) for the line; a list makes a head-to-tail gradient. Invalid entries fall back to black.

`FF0000`

![simpleLineOptions.color.values = FF0000](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`00A2FF,8B00FF`

![simpleLineOptions.color.values = 00A2FF,8B00FF](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`000000,FF0000,FFFFFF`

![simpleLineOptions.color.values = 000000,FF0000,FFFFFF](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`simpleLineOptions.color.blend_width`** — 0–100: how much of each transition blends (0 = hard bands, 100 = full gradient).

`0`

![simpleLineOptions.color.blend_width = 0](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`50`

![simpleLineOptions.color.blend_width = 50](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`100`

![simpleLineOptions.color.blend_width = 100](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`simpleLineOptions.color.interpolation`** — Blending curve: `linear`, `smoothstep`, `ease_in`, `ease_out`.

`linear`

![simpleLineOptions.color.interpolation = linear](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`smoothstep`

![simpleLineOptions.color.interpolation = smoothstep](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`ease_in`

![simpleLineOptions.color.interpolation = ease_in](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`ease_out`

![simpleLineOptions.color.interpolation = ease_out](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`simpleLineOptions.opacity.values`** — Comma-separated opacity percentages (0–100) from head to tail.

`100`

![simpleLineOptions.opacity.values = 100](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`100,0`

![simpleLineOptions.opacity.values = 100,0](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`100,0,100`

![simpleLineOptions.opacity.values = 100,0,100](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`simpleLineOptions.trail_origin_on_cursor_change`** — On cursor image change: `none` freezes the origin, `immediate` snaps, `smooth` glides there.

`none`

![simpleLineOptions.trail_origin_on_cursor_change = none](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`immediate`

![simpleLineOptions.trail_origin_on_cursor_change = immediate](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`smooth`

![simpleLineOptions.trail_origin_on_cursor_change = smooth](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`simpleLineOptions.antialiasing`** — Smooth the trail edges.

`on`

![simpleLineOptions.antialiasing = on](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`off`

![simpleLineOptions.antialiasing = off](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

### Cursor ghost options

**`ghostOptions.trail_mode`** — `time_based` or `size_based`: how the copies expire.

`time_based`

![ghostOptions.trail_mode = time_based](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`size_based`

![ghostOptions.trail_mode = size_based](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`ghostOptions.timeBased.tail_duration`** — Milliseconds each cursor copy stays visible (min 20).

`300`

![ghostOptions.timeBased.tail_duration = 300](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`1000`

![ghostOptions.timeBased.tail_duration = 1000](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`2000`

![ghostOptions.timeBased.tail_duration = 2000](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`ghostOptions.sizeBased.tail_size`** — Number of cursor copies in the trail (min 2, max 512).

`5`

![ghostOptions.sizeBased.tail_size = 5](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`20`

![ghostOptions.sizeBased.tail_size = 20](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`50`

![ghostOptions.sizeBased.tail_size = 50](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`ghostOptions.sizeBased.timeout`** — Milliseconds of inactivity before the copies fade, using the Time based duration (0 = disabled).

`0`

![ghostOptions.sizeBased.timeout = 0](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`1000`

![ghostOptions.sizeBased.timeout = 1000](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`5000`

![ghostOptions.sizeBased.timeout = 5000](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`ghostOptions.spacing`** — Extra distance in pixels between copies (0 = automatic, based on the copy count; max 200).

`0`

![ghostOptions.spacing = 0](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`10`

![ghostOptions.spacing = 10](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`40`

![ghostOptions.spacing = 40](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`ghostOptions.size.values`** — Comma-separated size multipliers from head to tail (1 = same, 0.8 = 80%). Avoid values above 1 (upscaled copies look pixelated).

`1`

![ghostOptions.size.values = 1](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`1,0`

![ghostOptions.size.values = 1,0](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`0.5,1`

![ghostOptions.size.values = 0.5,1](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`1,0.5,1,0.5`

![ghostOptions.size.values = 1,0.5,1,0.5](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`ghostOptions.opacity.values`** — Comma-separated opacity percentages (0–100) from head to tail.

`100`

![ghostOptions.opacity.values = 100](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`100,0`

![ghostOptions.opacity.values = 100,0](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`100,20,100,20`

![ghostOptions.opacity.values = 100,20,100,20](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`ghostOptions.color.values`** — Empty keeps the cursor's own colors; otherwise hex color(s) to recolor the pixels selected by Replace.

`(empty)`

![ghostOptions.color.values = empty](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`FFFFFF`

![ghostOptions.color.values = FFFFFF](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`00A2FF,8B00FF`

![ghostOptions.color.values = 00A2FF,8B00FF](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`ghostOptions.color.blend_width`** — 0–100: how much of each transition blends (0 = hard bands, 100 = full gradient).

`0`

![ghostOptions.color.blend_width = 0](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`50`

![ghostOptions.color.blend_width = 50](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`100`

![ghostOptions.color.blend_width = 100](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`ghostOptions.color.interpolation`** — Blending curve: `linear`, `smoothstep`, `ease_in`, `ease_out`.

`linear`

![ghostOptions.color.interpolation = linear](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`smoothstep`

![ghostOptions.color.interpolation = smoothstep](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`ease_in`

![ghostOptions.color.interpolation = ease_in](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`ease_out`

![ghostOptions.color.interpolation = ease_out](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`ghostOptions.color.replace.mode`** — Pixels to recolor: `auto` (enclosed center color), `custom` (the Custom color), or `whole` (every non-transparent pixel).

`auto`

![ghostOptions.color.replace.mode = auto](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`custom`

![ghostOptions.color.replace.mode = custom](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`whole`

![ghostOptions.color.replace.mode = whole](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`ghostOptions.color.replace.custom`** — Original cursor color to swap for `color.values` when mode is `custom` (e.g. `000000` for a black body).

`FFFFFF`

![ghostOptions.color.replace.custom = FFFFFF](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`000000`

![ghostOptions.color.replace.custom = 000000](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`808080`

![ghostOptions.color.replace.custom = 808080](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

### Enable/disable hotkey

**`hotkeyOptions.key`** — Global hotkey that toggles the trail on/off. Format `Modifier+Key`; at least one modifier (Ctrl, Alt, Shift, Win) is required. Empty disables the hotkey.

`(empty)`

![hotkeyOptions.key = empty](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`Ctrl+Alt+T`

![hotkeyOptions.key = Ctrl+Alt+T](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`hotkeyOptions.animate`** — Show a circle animation when the hotkey toggles the trail.

`on`

![hotkeyOptions.animate = on](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`off`

![hotkeyOptions.animate = off](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

The animation is a circle outline (2px, in the cursor's color, centered on the trail start and following the cursor): it grows and fades out when disabling, and shrinks and fades in when enabling.

`toggle effect`

![hotkey toggle effect](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

### Trail offset

**`tail_offset.x`** — Horizontal nudge of the trail origin in pixels (0 = auto-centered).

`-10`

![tail_offset.x = -10](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`0`

![tail_offset.x = 0](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`+10`

![tail_offset.x = +10](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

**`tail_offset.y`** — Vertical nudge of the trail origin in pixels (0 = auto-centered).

`-10`

![tail_offset.y = -10](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`0`

![tail_offset.y = 0](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`+10`

![tail_offset.y = +10](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

### Debug

**`debug.show_outline`** — Draw white (bitmap bounds) and red (visible pixels) outline boxes plus a green `+` at the trail start.

`off`

![debug.show_outline = off](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

`on`

![debug.show_outline = on](https://github.com/user-attachments/assets/e41ef1cb-33ff-412c-b707-58211402374d)

## Above the taskbar and Start menu

On Windows 11 the trail is drawn under the taskbar and Start menu. Install
the companion **Cursor trail helper - always on top** mod to lift it above
both (it needs a one-time Win-key press).

## Architecture

The main mod is a single translation unit (`CursorTrail.cpp`); the optional always-on-top helper (see [Above the taskbar](#above-the-taskbar-and-start-menu)) is a separate mod in `CursorTrailHelperAlwaysOnTop.cpp`. This section describes the main mod. All state is file-scope, grouped into five struct instances:

| Instance | Type | Purpose |
|---|---|---|
| `settings` | `Settings` | Parsed settings (tail geometry, style, width/color/opacity, origin mode). Written by `LoadSettings()`, read by all threads. |
| `cursor` | `CursorState` | Cursor geometry cache: `centerOffset`/`visualOffset`/`frozenOffset` (mutex-protected) plus render-thread-only debug dims and the per-`HCURSOR` `geomCache`. |
| `origin` | `OriginTransition` | Poll-thread-owned ease-in-out state for the trail-origin glide on cursor-image change. |
| `render` | `RenderResources` | Direct2D factory/target/brushes, stroke style, the cached backbuffer, and the per-`HCURSOR` `cursorBitmapCache` (tinted bitmap variants) used by the ghost style. Render-thread-only. |
| `runtime` | `Runtime` | Overlay window/threads, the `history` deque, atomics, multimedia timer, and per-frame render state. |

### Threads

- **Overlay thread** (`OverlayThreadProc`) — creates the `WS_EX_LAYERED` topmost window, runs the message loop, and does all Direct2D rendering via `SmearTimerProc`.
- **Poll thread** (`PollThreadProc`) — samples the cursor every 1 ms and pushes decimated samples into `runtime.history`.
- **Multimedia timer** (`MMTimerCallback`, a system thread) — posts `WM_TIMER` at ~125 Hz to wake the overlay thread. It never touches Direct2D directly.

### Locking model

- `runtime.historyMutex` protects `runtime.history` (poll + render threads).
- `cursor.offsetMutex` protects `cursor.centerOffset` / `cursor.visualOffset` / `cursor.frozenOffset` (written by render thread, read by poll thread).
- Lock order is always `runtime.historyMutex` → `cursor.offsetMutex`.
- `runtime.isGameRunning`, `runtime.cursorHidden`, `runtime.renderScheduled`, and `runtime.trailEnabled` are atomics.
- `origin.*`, `render.*`, and the cursor debug dimensions are single-thread owned (see table above).

### Render pipeline

`SmearTimerProc` is a thin orchestrator that delegates to helpers, in order:

1. `EnsureBackbuffer` / `EnsureRenderTarget` — (re)create the backbuffer bitmap and D2D render target.
2. `BuildTrailPoints` — snapshot `runtime.history`; the line style spatially decimates it, while the ghost style emits every latched copy in order, producing a parallel per-point `HCURSOR` list and (ghost only) a per-point opacity `ratio` list.
3. `ChaikinSmooth` (Simple line only) — two-pass corner smoothing; the ghost style draws its latched copies directly so its copy count matches the setting.
4. `ComputeTrailBBox` — trail bounding box plus stroke-width (line) or cursor-size (ghost) margin.
5. `RenderTrail` — dispatch to the active style renderer (`RenderSimpleLineStyle` or `RenderCursorGhostStyle`); both paint tail → head so the newest part stays on top at self-crossings.
6. `RenderToggleEffect` — optional enable/disable hotkey circle (2px outline, centered on the trail head, follows the cursor).
7. `DrawDebug` — optional white/red outline boxes plus a green trail-start marker.
8. `BlitOverlay` — dirty-rect tracking plus `UpdateLayeredWindow`.
9. `PruneCursorCaches` (ghost only) — drop cached cursor geometry/bitmaps no longer referenced by the trail.

### Settings & interpolation

- `LoadSettings` uses `ReadStringSetting`, `ParseFloatList`, `SplitAndTrim`, and `ParseHexColor`, and precomputes color band boundaries (`settings.colorBandStart`/`colorBandEnd`) and opacity alphas (`settings.opacityValues`, stored as 0–1) so the hot path does no parsing or per-frame allocation. `LoadCommonTrailSettings(prefix)` reads the settings shared by both styles (trail mode, tail duration/size, timeout, opacity) from `ghostOptions` or `simpleLineOptions`, and `LoadColorSettings(prefix, defaultColor)` reads the per-style color gradient into `settings.activeColorsRGB` (and precomputes the ghost tint samples into `settings.ghostTints`); `TrailPointBudget`/`AutoPointSpacing` hold the shared point-count and spacing formulas.
- `GetBlendedColor`, `InterpolateValues`, and `InterpolateOpacity` are allocation-free; `Ease` centralizes the easing curves (`linear`/`smoothstep`/`ease_in`/`ease_out`).

### Cursor geometry

`UpdateCursorCenterOffset` caches, per `HCURSOR`: the bitmap-center offset (for the debug boxes), the visible-pixel-center offset (the line-style trail origin), the hotspot, the alpha-trimmed visible bounds, and the DPI scale. It is rebuilt only when the cursor handle changes. `ComputeCursorGeom` holds the shared computation; `GetCursorGeom` lazily computes geometry for any cursor handle still referenced by the trail (so the ghost style can draw older images after an image change). Ghost samples store the raw cursor hotspot, and the ghost renderer anchors each copy by its own image's hotspot, so a copy lands exactly where that cursor image was — independent of the render thread's offset refresh. The ghost style also builds and caches D2D bitmaps per `HCURSOR` via `EnsureCursorBitmap`, rendering the cursor with `DrawIconEx` at the on-screen pixel size (color + mask + anti-aliased alpha) so copies are blitted 1:1 without resampling. The `ghostOptions.color` gradient (when set) is baked into the cached pixels by sampling it at `kGhostTintSteps` ratios (one bitmap per sample; an empty color list yields a single untinted variant), and `GetCursorBitmap(hCursor, ratio)` selects the nearest variant for each copy. `color.replace.mode` picks the pixels to recolor: `whole` swaps every non-transparent pixel; `auto` and `custom` split each pixel between two reference colors — the replace color (`auto` uses the cursor's enclosed center color from `AnalyzeCursorColors`, falling back to the largest region when nothing is enclosed; `custom` uses `color.replace.custom`) and a keep color (the largest boundary/outline region, or the region farthest from the replace color) — swapping pixels closer to the replace color with a small softness band around the midpoint, so anti-aliased transitions split cleanly instead of leaving a halo. Both caches are released with the render target.

### Lifecycle

- `WhTool_ModInit` — `LoadSettings()` then spawns `OverlayThreadProc`.
- `WhTool_ModSettingsChanged` — `LoadSettings()`, then posts `kMsgApplyHotkey` to the overlay window to re-register the hotkey on its thread.
- `WhTool_ModUninit` — signals the poll thread, kills the timer, and posts `WM_QUIT`.
- The overlay thread registers the `hotkeyOptions.key` setting (`ApplyHotkey`) right after creating the window and unregisters it before destroying the window. `WM_HOTKEY` flips `runtime.trailEnabled`, which suppresses sampling/rendering like the fullscreen-game path does, and (when `hotkeyOptions.animate` is on) calls `StartToggleEffect` to play the circle animation centered on the trail head (disable: grows + fades out, ease in; enable: shrinks + fades in, ease out; 400 ms, 2px outline, diameter 6× the cursor, colored by `GetCursorColor`'s ghost-`auto` pick, following the cursor).
- The `Wh_ModInit` / `Wh_ModAfterInit` / `Wh_ModUninit` block at the bottom of the file is Windhawk's tool-mod launcher boilerplate and should be left as-is.
