# WindHawk - Cursor Trail

A [Windhawk](https://windhawk.net) mod that renders a customizable cursor trail (motion blur) overlay on the Windows desktop using Direct2D.

## Styles

- **Simple line** — A thin polyline with configurable width, color(s), gradient stops, and per-segment opacity.

## How it works

- A high-frequency polling thread samples cursor position every 1 ms.
- A render thread draws sampled points via a layered (`WS_EX_LAYERED`) topmost transparent window, using Direct2D with per-pixel alpha via `UpdateLayeredWindow`.
- Point positions are spatially decimated and smoothed with Chaikin subdivision before rendering.
- Trail segments automatically expire after the configured tail duration. The overlay is paused when a fullscreen exclusive (game) application is detected.
- When the cursor is hidden (e.g. Windows' hide-while-typing), the trail fades out over the tail duration in both trail modes.

## Architecture

Single translation unit (`CursorTrail.cpp`). All state is file-scope, grouped into five struct instances:

| Instance | Type | Purpose |
|---|---|---|
| `settings` | `Settings` | Parsed settings (tail geometry, style, width/color/opacity, origin mode). Written by `LoadSettings()`, read by all threads. |
| `cursor` | `CursorState` | Cursor geometry cache: `centerOffset`/`visualOffset`/`frozenOffset` (mutex-protected) plus render-thread-only debug dims. |
| `origin` | `OriginTransition` | Poll-thread-owned ease-in-out state for the trail-origin glide on cursor-image change. |
| `render` | `RenderResources` | Direct2D factory/target/brushes, stroke style, and the cached backbuffer. Render-thread-only. |
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
2. `BuildTrailPoints` — snapshot `runtime.history` and spatially decimate.
3. `ChaikinSmooth` — two-pass corner smoothing.
4. `ComputeTrailBBox` — trail bounding box plus stroke-width margin.
5. `RenderTrail` — dispatch to the active style renderer (currently only `RenderSimpleLineStyle`; `cursor_ghost` is reserved and renders nothing).
6. `DrawDebug` — optional white/red outline boxes plus a blue trail-start marker.
7. `BlitOverlay` — dirty-rect tracking plus `UpdateLayeredWindow`.

### Settings & interpolation

- `LoadSettings` uses `ReadStringSetting`, `ParseFloatList`, `SplitAndTrim`, and `ParseHexColor`, and precomputes color band boundaries (`settings.colorBandStart`/`colorBandEnd`) and opacity alphas (`settings.simpleLineOpacityValues`, stored as 0–1) so the hot path does no parsing or per-frame allocation.
- `GetBlendedColor`, `InterpolateWidth`, and `InterpolateOpacity` are allocation-free; `Ease` centralizes the easing curves (`linear`/`smoothstep`/`ease_in`/`ease_out`).

### Cursor geometry

`UpdateCursorCenterOffset` caches, per `HCURSOR`: the bitmap-center offset (for the debug boxes), the visible-pixel-center offset (the trail origin), the alpha-trimmed visible bounds, and the DPI scale. It is rebuilt only when the cursor handle changes.

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
