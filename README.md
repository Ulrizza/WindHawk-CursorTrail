# WindHawk - Cursor Trail

A [Windhawk](https://windhawk.net) mod that renders a customizable cursor trail (motion blur) overlay on the Windows desktop using Direct2D.

## Styles

- **Simple line** — A thin polyline with configurable width, color(s), gradient stops, and per-segment opacity.
- **Cursor ghost** — Faded copies of the cursor image. Each copy is latched at the screen position where it was spawned and never moves; it only fades/expires. Each copy also keeps the exact cursor image from when it was sampled, so an image change (e.g. arrow to I-beam) appears gradually along the trail.

## How it works

- A high-frequency polling thread samples cursor position every 1 ms.
- A render thread draws sampled points via a layered (`WS_EX_LAYERED`) topmost transparent window, using Direct2D with per-pixel alpha via `UpdateLayeredWindow`.
- For the line style, point positions are spatially decimated and smoothed with Chaikin subdivision before rendering. For the ghost style, the poll thread only pushes a sample once the cursor has travelled the configured spawn distance, so each copy is latched at a fixed screen position; the renderer draws them without further decimation.
- Trail segments automatically expire after the configured tail duration. The overlay is paused when a fullscreen exclusive (game) application is detected.
- When the cursor is hidden (e.g. Windows' hide-while-typing), the trail fades out over the tail duration in both trail modes.

## Architecture

Single translation unit (`CursorTrail.cpp`). All state is file-scope, grouped into five struct instances:

| Instance | Type | Purpose |
|---|---|---|
| `settings` | `Settings` | Parsed settings (tail geometry, style, width/color/opacity, origin mode). Written by `LoadSettings()`, read by all threads. |
| `cursor` | `CursorState` | Cursor geometry cache: `centerOffset`/`visualOffset`/`frozenOffset` (mutex-protected) plus render-thread-only debug dims and the per-`HCURSOR` `geomCache`. |
| `origin` | `OriginTransition` | Poll-thread-owned ease-in-out state for the trail-origin glide on cursor-image change. |
| `render` | `RenderResources` | Direct2D factory/target/brushes, stroke style, the cached backbuffer, and the per-`HCURSOR` `cursorBitmapCache` used by the ghost style. Render-thread-only. |
| `runtime` | `Runtime` | Overlay window/threads, the `history` deque, atomics, multimedia timer, and per-frame render state. |

### Threads

- **Overlay thread** (`OverlayThreadProc`) — creates the `WS_EX_LAYERED` topmost window, runs the message loop, and does all Direct2D rendering via `SmearTimerProc`.
- **Poll thread** (`PollThreadProc`) — samples the cursor every 1 ms and pushes decimated samples into `runtime.history`.
- **Multimedia timer** (`MMTimerCallback`, a system thread) — posts `WM_TIMER` at ~125 Hz to wake the overlay thread. It never touches Direct2D directly.

### Locking model

- `runtime.historyMutex` protects `runtime.history` (poll + render threads).
- `cursor.offsetMutex` protects `cursor.centerOffset` / `cursor.visualOffset` / `cursor.frozenOffset` (written by render thread, read by poll thread).
- Lock order is always `runtime.historyMutex` → `cursor.offsetMutex`.
- `runtime.isGameRunning`, `runtime.cursorHidden`, and `runtime.renderScheduled` are atomics.
- `origin.*`, `render.*`, and the cursor debug dimensions are single-thread owned (see table above).

### Render pipeline

`SmearTimerProc` is a thin orchestrator that delegates to helpers, in order:

1. `EnsureBackbuffer` / `EnsureRenderTarget` — (re)create the backbuffer bitmap and D2D render target.
2. `BuildTrailPoints` — snapshot `runtime.history`; the line style spatially decimates it, while the ghost style emits every latched copy in order, producing a parallel per-point `HCURSOR` list and (ghost only) a per-point opacity `ratio` list.
3. `ChaikinSmooth` (Simple line only) — two-pass corner smoothing; the ghost style draws its latched copies directly so its copy count matches the setting.
4. `ComputeTrailBBox` — trail bounding box plus stroke-width (line) or cursor-size (ghost) margin.
5. `RenderTrail` — dispatch to the active style renderer (`RenderSimpleLineStyle` or `RenderCursorGhostStyle`).
6. `DrawDebug` — optional white/red outline boxes plus a blue trail-start marker.
7. `BlitOverlay` — dirty-rect tracking plus `UpdateLayeredWindow`.
8. `PruneCursorCaches` (ghost only) — drop cached cursor geometry/bitmaps no longer referenced by the trail.

### Settings & interpolation

- `LoadSettings` uses `ReadStringSetting`, `ParseFloatList`, `SplitAndTrim`, and `ParseHexColor`, and precomputes color band boundaries (`settings.colorBandStart`/`colorBandEnd`) and opacity alphas (`settings.opacityValues`, stored as 0–1) so the hot path does no parsing or per-frame allocation. `LoadCommonTrailSettings(prefix)` reads the settings shared by both styles (trail mode, tail duration/size, timeout, opacity) from `ghostOptions` or `simpleLineOptions`; `TrailPointBudget`/`AutoPointSpacing` hold the shared point-count and spacing formulas.
- `GetBlendedColor`, `InterpolateValues`, and `InterpolateOpacity` are allocation-free; `Ease` centralizes the easing curves (`linear`/`smoothstep`/`ease_in`/`ease_out`).

### Cursor geometry

`UpdateCursorCenterOffset` caches, per `HCURSOR`: the bitmap-center offset (for the debug boxes), the visible-pixel-center offset (the line-style trail origin), the hotspot, the alpha-trimmed visible bounds, and the DPI scale. It is rebuilt only when the cursor handle changes. `ComputeCursorGeom` holds the shared computation; `GetCursorGeom` lazily computes geometry for any cursor handle still referenced by the trail (so the ghost style can draw older images after an image change). Ghost samples store the raw cursor hotspot, and the ghost renderer anchors each copy by its own image's hotspot, so a copy lands exactly where that cursor image was — independent of the render thread's offset refresh. The ghost style also builds and caches a D2D bitmap per `HCURSOR` via `EnsureCursorBitmap`, rendering the cursor with `DrawIconEx` at the on-screen pixel size (color + mask + anti-aliased alpha) so copies are blitted 1:1 without resampling; both caches are released with the render target.

### Lifecycle

- `WhTool_ModInit` — `LoadSettings()` then spawns `OverlayThreadProc`.
- `WhTool_ModSettingsChanged` — `LoadSettings()`.
- `WhTool_ModUninit` — signals the poll thread, kills the timer, and posts `WM_QUIT`.
- The `Wh_ModInit` / `Wh_ModAfterInit` / `Wh_ModUninit` block at the bottom of the file is Windhawk's tool-mod launcher boilerplate and should be left as-is.

## Settings

| Setting | Description |
|---|---|
| `style` | Rendering style: `simple_line` or `cursor_ghost` |
| `simpleLineOptions.trail_mode` | `time_based` (default) or `size_based` — how the trail expires |
| `simpleLineOptions.antialiasing` | `True` (default) or `False` — smooth trail edges or hard, pixelated edges |
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
| `ghostOptions.sizeBased.tail_size` | Number of cursor copies in the trail, size-based mode (min 2) |
| `ghostOptions.sizeBased.timeout` | Milliseconds of inactivity before the copies fade using the Time based tail duration (0 = disabled) |
| `ghostOptions.spacing` | Extra distance in pixels added between cursor copies. A new copy is stamped each time the cursor travels this gap (0 = automatic). |
| `ghostOptions.size.values` | Comma-separated size multipliers from head to tail (1 = same size, 0.8 = 80%, 2 = twice). Each value gets an equal share; repeat to widen. Avoid values above 1 (upscaled copies look pixelated); use the Windows cursor size setting to enlarge the cursor |
| `ghostOptions.opacity.values` | Comma-separated opacity percentages (0-100) from head to tail, each gets equal share |
| `waveform.type` | Wave pattern applied to the trail: `none`, `sinus`, `square`, or `triangle` |
| `waveform.amplitude` | Maximum pixel offset applied by the waveform (0 = disabled) |
| `waveform.period` | Milliseconds per full wave cycle (lower = faster wobble) |
| `debug.show_outline` | `False` (default) — draw white (bitmap bounds) and red (visible pixels) outline boxes around the cursor, plus a blue `+` at the trail start |

## Building

Requires the Windhawk SDK. Link against `d2d1`, `ole32`, `gdi32`, `shell32`, `windowscodecs`, `winmm`, and `shcore`.

```
# From the Windhawk mod directory:
cl /EHsc /O2 CursorTrail.cpp /link d2d1.lib ole32.lib gdi32.lib shell32.lib windowscodecs.lib winmm.lib shcore.lib
```
