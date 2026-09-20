# Changelog

## 0.17
- Settings UX pass before release:
  - `simpleLineOptions.antialiasing` is now a boolean switch instead of a True/False dropdown.
  - Fixed `%%` rendering literally in the Width and Size descriptions (now `%`).
  - Reordered Simple line options to trail mode, tail geometry, appearance, then advanced options (Trail origin, Antialiasing).
  - Added group descriptions noting which Style and which Trail mode each group applies to, and that Replace is only used when Color > Values is set.
  - Clarified the copy-count max (512), copy-spacing max (200), invalid-color fallback, and that Blend width / Interpolation need two or more colors.
  - Changed the Simple line default color from a 9-color rainbow to a two-stop `00A2FF,8B00FF` gradient.
  - Renamed the "Values" labels to "Values (head to tail)" and the Auto replace mode label to "Auto (center color)".
  - Multi-choice settings (Style, Trail mode, Trail origin, Interpolation, Replace mode) now describe each choice as a `-` bullet on its own line instead of one inline sentence.
  - README: removed the non-existent `waveform.*` settings and added the missing `tail_offset.x` / `tail_offset.y` rows.
- Reworked the mod description (Windhawk readme): one-phrase summary, a short summary of both styles, a trimmed features list, and a placeholder GIF per example value (79 examples across all 27 settings, no table). Updated `@description` to match and corrected the tagline in the description and README.
- Added a customizable enable/disable hotkey, grouped under `hotkeyOptions` with `hotkeyOptions.key` (a `Modifier+Key` string, e.g. `Ctrl+Alt+T`; at least one modifier required; empty = disabled by default) and `hotkeyOptions.animate` (switch, default on). The key is parsed by `ParseHotkey`/`VkFromKeyName` and registered on the overlay window with `RegisterHotKey(..., MOD_NOREPEAT)`; `WM_HOTKEY` flips the new `runtime.trailEnabled` atomic, which suppresses sampling and clears/renders nothing like the fullscreen-game path. Re-registered on the overlay thread via `kMsgApplyHotkey` on settings change, unregistered on unload. Documented with example GIFs.
- Added a circle animation on hotkey toggle (`ToggleEffect` / `StartToggleEffect` / `RenderToggleEffect`), controlled by `hotkeyOptions.animate`: a 2px outline in the cursor's color (`GetCursorColor`, using the ghost `auto` pick) for 400 ms, centered on the trail head (`smoothed[0]`, so it matches the start of the trail) and following the cursor. Disabling grows the circle (diameter up to 6× the cursor size) while fading out with ease-in; enabling shrinks it while fading in with ease-out. Drawn on the overlay thread and included in the dirty-rect bbox.
- Fixed the trail painting older parts over newer ones at self-crossings: both style renderers now draw tail → head so the newest segment/copy is on top.
- Changed the Cursor ghost defaults: `ghostOptions.timeBased.tail_duration` 500 → 300 ms and `ghostOptions.opacity.values` "100,20" → "50,20".
- Reordered the settings in the mod description and README to mirror the Windhawk settings page exactly: `Style`, `Simple line options`, `Cursor ghost options`, `Enable/disable hotkey`, `Trail offset`, `Debug` (README settings split into the same per-group tables).

## 0.16
- Debug: Show outline now draws 2px boxes, a green trail-start `+` (was blue) with longer arms, and 2px `+` strokes.

## 0.15
- Added a Color setting to Cursor ghost (`ghostOptions.color.values`, `blend_width`, `interpolation`): a single hex color or a comma-separated list for a head-to-tail gradient. Empty (the default) keeps the cursor's own colors.
- Added a Replace subgroup (`ghostOptions.color.replace`) with a `mode` selector and a `custom` color. `auto` (default) recolors the cursor's enclosed center color, ignoring the outline/contour (`AnalyzeCursorColors`; falls back to the largest region when nothing is enclosed); `custom` recolors the `custom` color (default `FFFFFF`; set `000000` to recolor a black cursor body); `whole` recolors every non-transparent pixel.
- Recoloring splits each pixel between the replace color and a keep color (the largest boundary/outline region, or the region farthest from the replace color) with a soft midpoint band, so anti-aliased edges no longer leave a halo of the original color.
- Colors are parsed via the shared `LoadColorSettings(prefix, defaultColor)` helper into `settings.activeColorsRGB` (empty means "no color" when `defaultColor` is null), and `EnsureCursorBitmap` bakes the sampled gradient into per-cursor bitmap variants selected by `GetCursorBitmap(hCursor, ratio)`.

## Cursor trail helper - always on top 1.0
- Added the **Cursor trail helper - always on top** companion mod (`CursorTrailBand.cpp`, mod id `cursor-trail-helper-always-on-top`). It runs inside `explorer.exe` and moves the overlay into `ZBID_SYSTEM_TOOLS` via the undocumented `SetWindowBand` API, so the trail draws above the Windows 11 taskbar and Start menu. When direct banding is denied it captures the IAM access key by hooking `NtUserEnableIAMAccess` (unhooked once captured), and re-applies the band if it is reset or the overlay is recreated.

## 0.14
- Fixed Cursor ghost copies sliding along with the cursor. Copies are now latched at the screen position where they are spawned (a new copy is stamped once the cursor travels the configured spacing) and only fade/expire in place.
- Ghost time-based opacity now fades by age; size-based copies keep a fixed count that stays put when the cursor stops.
- Fixed the first ghost copies after a cursor image change (e.g. arrow to I-beam) being misaligned: ghost samples now store the raw cursor hotspot and each copy is anchored by its own image's hotspot, instead of using a trail-origin offset that lagged behind the image change.
- Added a Timeout setting to Cursor ghost size-based mode (`ghostOptions.sizeBased.timeout`, default 2000) that fades the copies after inactivity, matching the simple line timeout. Any cursor movement resets it.
- Refactored settings loading: both styles now read their shared trail settings (trail mode, tail duration/size, timeout, opacity) through `LoadCommonTrailSettings(prefix)`, and the point-count/spacing formulas are shared helpers (`TrailPointBudget`, `AutoPointSpacing`). Renamed `Settings::simpleLineOpacityValues` to `opacityValues` since both styles use it.
- Added a Size setting to Cursor ghost (`ghostOptions.size.values`): comma-separated size multipliers from head to tail (1 = same size, 2 = twice, 0.8 = 80%), interpolated like the line width and anchored on the copy's hotspot. Renamed `InterpolateWidth` to `InterpolateValues` since it is shared by width, opacity, and size.

## 0.13
- Removed the Cursor ghost style implementation; the `cursor_ghost` setting is retained but renders nothing (reserved for a future rework). Debug outline dimensions now come from `UpdateCursorCenterOffset`.
- Refactored globals into `Settings`, `CursorState`, `OriginTransition`, `RenderResources`, and `Runtime` structs, and split `SmearTimerProc` into focused helpers.
- Added shared helpers (`ReadStringSetting`, `ParseFloatList`, `Ease`, `EvictByTime`, `RoundToLong`, `ReleaseRenderTargetResources`, `GrowBBox`) to remove duplicated logic.
- Performance: allocation-free interpolation, precomputed color band boundaries and opacity alphas, and enum/bool state instead of per-frame string comparisons.
- Documented the code architecture in the README.

## 0.12
- Added "Trail origin on cursor change" setting for the Simple line style: `none` keeps the trail origin frozen, `immediate` snaps to the new cursor's visual center, and `smooth` (default) glides there with an ease-in-out transition when the cursor image changes (e.g. arrow to I-beam).

## 0.11
- Fixed opacity not visibly changing: brush colors now use premultiplied alpha to match the D2D render target pixel format, so per-segment opacity values render correctly.

## 0.10
- Grouped Tail duration under a "Time based" container and Tail length under a "Size based" container.
- Added timeout setting to Size based mode: trail fades smoothly using the Time based tail duration after cursor is stationary for the configured time (0 = never).
- Changed default tail_duration to 500ms and tail_size to 200.

## 0.9
- Removed percentage settings; each value now gets an equal share of the trail (duplicate to widen).
- Simplified color system: replaced smooth_gradient toggle + two functions with single blend_width slider (0-100) and interpolation selector (linear, smoothstep, ease_in, ease_out).
- Grouped trail offset under a Tail offset group.
- Fixed GetBlendedColor zone clamping and edge-case bugs.

## 0.8
- Width, color, and opacity settings now use comma-separated value/percentage lists with smoothstep interpolation, group-organized under simpleLineOptions.
- Added trail mode selector (time_based / size_based) with configurable tail length.
- Fixed color gradient off-by-one bug causing shifted stops.
- Fixed even-spacing formula for gradient stops (n-1 denominator).

## 0.7
- Replaced single simple_line_width with separate trail_start / trail_end width settings, interpolated per-segment (like opacity).

## 0.6
- Fixed tail freeze when cursor stops: increased sample cap and spatial decimation budget to match tail_duration so eviction reaches the visible tail immediately.

## 0.5
- Pre-parsed hex colors into cached Rgb structs to avoid string scanning in the render loop.
- Cached the Simple Line style's ID2D1SolidColorBrush, eliminating per-frame create/destroy.
- Optimized Chaikin subdivision with reserve + swap to avoid vector reallocation/copy.

## 0.4
- Extracted SplitAndTrim, ResolveCursorBitmapDimensions, FreeIconInfoBitmaps, ClampAlpha, and Clamp01 helpers to eliminate duplicated parsing and clamping logic.

## 0.3
- Removed dead code: g_lastPos global, empty if-block, unused r/g/b locals.

## 0.2
- Removed the Cartoon rendering style and its associated brushes and geometry code.

## 0.1
- Initial release. Three styles: Cartoon, Simple Line, Cursor Ghost.
