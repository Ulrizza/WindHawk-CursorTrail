# WindHawk - Cursor Trail

A [Windhawk](https://windhawk.net) mod that renders a customizable cursor trail (motion blur) overlay on the Windows desktop using Direct2D.

## Styles

- **Simple line** — A thin polyline with configurable width, color(s), gradient stops, and per-segment opacity.
- **Cursor ghost** — Stamped copies of the current cursor icon along the trail path, fading out toward the tail.

## How it works

- A high-frequency polling thread samples cursor position every 1 ms.
- A render thread draws sampled points via a layered (`WS_EX_LAYERED`) topmost transparent window, using Direct2D with per-pixel alpha via `UpdateLayeredWindow`.
- Point positions are spatially decimated and smoothed with Chaikin subdivision before rendering.
- Trail segments automatically expire after the configured tail duration. The overlay is paused when a fullscreen exclusive (game) application is detected.

## Settings

| Setting | Description |
|---|---|
| `style` | Rendering style: `simple_line` or `cursor_ghost` |
| `simpleLineOptions.trail_mode` | `time_based` (default) or `size_based` — how the trail expires |
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

## Building

Requires the Windhawk SDK. Link against `d2d1`, `ole32`, `gdi32`, `shell32`, `windowscodecs`, and `winmm`.

```
# From the Windhawk mod directory:
cl /EHsc /O2 CursorStuff.cpp /link d2d1.lib ole32.lib gdi32.lib shell32.lib windowscodecs.lib winmm.lib
```
