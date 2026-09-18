# Changelog

## Unreleased
- Fixed Cursor ghost copies sliding along with the cursor. Copies are now latched at the screen position where they are spawned (a new copy is stamped once the cursor travels the configured spacing) and only fade/expire in place.
- Ghost time-based opacity now fades by age; size-based copies keep a fixed count that stays put when the cursor stops.
- Fixed the first ghost copies after a cursor image change (e.g. arrow to I-beam) being misaligned: ghost samples now store the raw cursor hotspot and each copy is anchored by its own image's hotspot, instead of using a trail-origin offset that lagged behind the image change.
- Added a Timeout setting to Cursor ghost size-based mode (`ghostOptions.sizeBased.timeout`, default 2000) that fades the copies after inactivity, matching the simple line timeout. Any cursor movement resets it.
- Refactored settings loading: both styles now read their shared trail settings (trail mode, tail duration/size, timeout, opacity) through `LoadCommonTrailSettings(prefix)`, and the point-count/spacing formulas are shared helpers (`TrailPointBudget`, `AutoPointSpacing`). Renamed `Settings::simpleLineOpacityValues` to `opacityValues` since both styles use it.

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
