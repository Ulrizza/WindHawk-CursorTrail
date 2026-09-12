# Changelog

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
