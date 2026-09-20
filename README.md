# WindHawk - Cursor Trail

A [Windhawk](https://windhawk.net) mod that renders a customizable cursor trail overlay on the Windows desktop using Direct2D.

## Styles

- **Simple line** — A thin polyline with configurable width, color(s), gradient stops, and per-segment opacity.
- **Cursor ghost** — Faded copies of the cursor image. Each copy is latched at the screen position where it was spawned and never moves; it only fades/expires. Each copy also keeps the exact cursor image from when it was sampled, so an image change (e.g. arrow to I-beam) appears gradually along the trail.

## How it works

- A high-frequency polling thread samples cursor position every 1 ms.
- A render thread draws sampled points via a layered (`WS_EX_LAYERED`) topmost transparent window, using Direct2D with per-pixel alpha via `UpdateLayeredWindow`.
- For the line style, point positions are spatially decimated and smoothed with Chaikin subdivision before rendering. For the ghost style, the poll thread only pushes a sample once the cursor has travelled the configured spawn distance, so each copy is latched at a fixed screen position; the renderer draws them without further decimation.
- Trail segments automatically expire after the configured tail duration. The overlay is paused when a fullscreen exclusive (game) application is detected.
- On Windows 11 the overlay cannot cover the taskbar or Start menu by itself: since Windows 8, windows live in fixed Z-order bands (`ZBID`), and the taskbar (`ZBID_IMMERSIVE_MOGO`, 6) and Start menu sit in bands above the desktop band (1) regardless of `WS_EX_TOPMOST`. The optional **Cursor trail helper - always on top** companion mod (see below) moves the overlay into `ZBID_SYSTEM_TOOLS` (16) so it draws above them.
- When the cursor is hidden (e.g. Windows' hide-while-typing), the trail fades out over the tail duration in both trail modes.

## Architecture

The main mod is a single translation unit (`CursorTrail.cpp`); the optional always-on-top helper (see [Above the taskbar](#above-the-taskbar-companion-mod)) is a separate mod in `CursorTrailBand.cpp`. This section describes the main mod. All state is file-scope, grouped into five struct instances:

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

## Settings

| Setting | Description |
|---|---|
| `style` | Rendering style: `simple_line` or `cursor_ghost` |
| `hotkeyOptions.key` | Global hotkey that toggles the trail on/off, e.g. `Ctrl+Alt+T`. At least one modifier (Ctrl/Alt/Shift/Win) is required; empty (default) disables the hotkey. Registered on the overlay window via `RegisterHotKey` (with `MOD_NOREPEAT`), so it fails silently if another app already owns the combo. |
| `hotkeyOptions.animate` | Switch (default on) — play the circle animation when the hotkey toggles the trail. |
| `simpleLineOptions.trail_mode` | `time_based` (default) or `size_based` — how the trail expires |
| `simpleLineOptions.antialiasing` | Switch (default on) — smooth trail edges or hard, pixelated edges |
| `simpleLineOptions.trail_origin_on_cursor_change` | Behavior when the cursor image changes: `smooth` (default) glides to the new cursor center with an ease-in-out transition; `none` keeps the origin frozen; `immediate` snaps |
| `simpleLineOptions.timeBased.tail_duration` | Milliseconds each trail segment stays visible (min 20) |
| `simpleLineOptions.sizeBased.tail_size` | Total trail length in pixels — eviction walks from head and drops points past this distance (min 20) |
| `simpleLineOptions.sizeBased.timeout` | Milliseconds of inactivity before trail fades using Time based duration (0 = disabled) |
| `simpleLineOptions.width.values` | Comma-separated stroke widths. Each value gets an equal share; repeat to widen (e.g. "2,2,2,2,1") |
| `simpleLineOptions.color.values` | Hex color(s) for the line (comma-separated), each gets equal share |
| `simpleLineOptions.color.blend_width` | 0-100: how much of each transition blends (0 = hard bands, 100 = full gradient) |
| `simpleLineOptions.color.interpolation` | Blending curve: linear, smoothstep, ease_in, ease_out |
| `simpleLineOptions.opacity.values` | Comma-separated opacity percentages (0-100), each gets equal share |
| `ghostOptions.trail_mode` | `time_based` (default) or `size_based` — how the copies expire |
| `ghostOptions.timeBased.tail_duration` | Milliseconds each cursor copy stays visible (min 20) |
| `ghostOptions.sizeBased.tail_size` | Number of cursor copies in the trail, size-based mode (min 2, max 512) |
| `ghostOptions.sizeBased.timeout` | Milliseconds of inactivity before the copies fade using the Time based tail duration (0 = disabled) |
| `ghostOptions.spacing` | Extra distance in pixels added between cursor copies. A new copy is stamped each time the cursor travels this gap (0 = automatic, based on the copy count; max 200). |
| `ghostOptions.size.values` | Comma-separated size multipliers from head to tail (1 = same size, 0.8 = 80%, 2 = twice). Each value gets an equal share; repeat to widen. Avoid values above 1 (upscaled copies look pixelated); use the Windows cursor size setting to enlarge the cursor |
| `ghostOptions.opacity.values` | Comma-separated opacity percentages (0-100) from head to tail, each gets equal share |
| `ghostOptions.color.values` | Hex color(s) for the cursor copies (comma-separated), each gets equal share. Empty (default) keeps the cursor's own colors; otherwise pixels matching the Replace color are recolored to this value |
| `ghostOptions.color.replace.mode` | Which pixels to recolor: `auto` (default, the cursor's enclosed center color — ignores the outline; falls back to the largest area if nothing is enclosed), `custom` (the `color.replace.custom` color), or `whole` (every non-transparent pixel) |
| `ghostOptions.color.replace.custom` | Original cursor color to swap for `color.values` when mode is `custom` (default `FFFFFF`, e.g. the white outline). Set `000000` to recolor a black cursor body |
| `ghostOptions.color.blend_width` | 0-100: how much of each transition blends (0 = hard bands, 100 = full gradient) |
| `ghostOptions.color.interpolation` | Blending curve: linear, smoothstep, ease_in, ease_out |
| `tail_offset.x` | Horizontal nudge of the trail origin in pixels (0 = auto-centered) |
| `tail_offset.y` | Vertical nudge of the trail origin in pixels (0 = auto-centered) |
| `debug.show_outline` | `False` (default) — draw white (bitmap bounds) and red (visible pixels) outline boxes around the cursor, plus a green `+` at the trail start |

## Above the taskbar (companion mod)

The trail is a normal desktop-band window, so on Windows 11 it is drawn *under* the taskbar and Start menu. To lift it above them, install the companion **Cursor trail helper - always on top** mod (`CursorTrailBand.cpp`, mod id `cursor-trail-helper-always-on-top`) alongside this one.

- It is injected into `explorer.exe` and moves the overlay to `ZBID_SYSTEM_TOOLS` (16) — the same band Task Manager's "Always on top" uses — via the undocumented `SetWindowBand` API.
- It finds the overlay by its window class name (`SmearFrameOverlayClass`), so the two mods have no runtime coupling other than that name.
- If direct banding is denied, it captures the IAM access key by hooking `NtUserEnableIAMAccess`; that requires pressing the **Win key** (or opening a shell surface) once after load.
- Caveats: undocumented APIs; the trail then also draws above Task Manager and Alt-Tab (it stays click-through). Enable/disable it independently of the main mod.

## Building

Requires the Windhawk SDK. Link against `d2d1`, `ole32`, `gdi32`, `shell32`, `windowscodecs`, `winmm`, and `shcore`.

```
# From the Windhawk mod directory:
cl /EHsc /O2 CursorTrail.cpp /link d2d1.lib ole32.lib gdi32.lib shell32.lib windowscodecs.lib winmm.lib shcore.lib
```

The always-on-top helper (`CursorTrailBand.cpp`) needs no extra libraries.
