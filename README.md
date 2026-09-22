# WindHawk - Cursor Trail

A fully customizable cursor trail for the Windows desktop.  

## The two styles

- **Simple line**: a polyline that follows the cursor; its width, color, and
  opacity can change from head to tail.
- **Cursor ghost**: faded copies of the cursor image, each latched at the spot
  where it spawned (they stay put and only fade out). Each copy keeps the exact
  cursor image from when it was sampled, so an image change (e.g. arrow to
  I-beam) appears gradually along the trail.

## Features

- **Time based** vs **Size based** trails.
- **Timing**: tail duration and inactivity timeout.
- **Size**: stroke width (Simple line) and copy size (Cursor ghost).
- **Color**: head-to-tail gradients, blend width, and ghost
  recoloring.
- **Hotkey**: toggle the trail on/off with a customizable global hotkey.

## Settings

### Style

Type of trail.

`style: simple_line`: a line whose width, color, and opacity can change along its length  
![style = simple_line](./images/style.simple_line.gif)

`style: cursor_ghost`: faded copies of the cursor image, each latched where it spawned  
![style = cursor_ghost](./images/style.cursor_ghost.gif)

### Simple line options

Applies when Style is Simple line.

#### Trail mode

How the trail disappears.

`simpleLineOptions.trail_mode: time_based`: each part of the trail fades after the Tail duration  
![simpleLineOptions.trail_mode = time_based](./images/simpleLineOptions.trail_mode.time_based.gif)

`simpleLineOptions.trail_mode: size_based`: keeps a fixed trail length even when the cursor stops  
![simpleLineOptions.trail_mode = size_based](./images/simpleLineOptions.trail_mode.size_based.gif)

#### Time based

Applies when Trail mode is Time based.

##### Tail duration

How long each trail segment stays visible, in milliseconds. Minimum 20.

`100`  
![simpleLineOptions.timeBased.tail_duration = 100](./images/simpleLineOptions.timeBased.tail_duration.100.gif)

`300`  
![simpleLineOptions.timeBased.tail_duration = 300](./images/simpleLineOptions.timeBased.tail_duration.300.gif)

`1000`  
![simpleLineOptions.timeBased.tail_duration = 1000](./images/simpleLineOptions.timeBased.tail_duration.1000.gif)

#### Size based

Applies when Trail mode is Size based.

##### Tail length

Maximum trail length in pixels. Minimum 20.

`100`  
![simpleLineOptions.sizeBased.tail_size = 100](./images/simpleLineOptions.sizeBased.tail_size.100.gif)

`500`  
![simpleLineOptions.sizeBased.tail_size = 500](./images/simpleLineOptions.sizeBased.tail_size.500.gif)

`1000`  
![simpleLineOptions.sizeBased.tail_size = 1000](./images/simpleLineOptions.sizeBased.tail_size.1000.gif)

##### Timeout

Milliseconds of inactivity before the trail starts fading (using the Time based tail duration). 0 = trail always visible.

`0`  
![simpleLineOptions.sizeBased.timeout = 0](./images/simpleLineOptions.sizeBased.timeout.0.gif)

`500`  
![simpleLineOptions.sizeBased.timeout = 500](./images/simpleLineOptions.sizeBased.timeout.500.gif)

`1000`  
![simpleLineOptions.sizeBased.timeout = 1000](./images/simpleLineOptions.sizeBased.timeout.1000.gif)

#### Width

##### Values (head to tail)

Comma-separated stroke widths in pixels from head to tail (e.g. "2,1" for a tapered trail, or "10,1,10,1" for a pulsing trail). Each value gets an equal share; repeat to widen (e.g. "2,2,2,2,1" = 80% at 2, 20% at 1). Minimum 1.

`5`  
![simpleLineOptions.width.values = 5](./images/simpleLineOptions.width.values.5.gif)

`5,0`  
![simpleLineOptions.width.values = 5,0](./images/simpleLineOptions.width.values.5.0.gif)

`1,1,1,10`  
![simpleLineOptions.width.values = 1,1,1,10](./images/simpleLineOptions.width.values.1.1.1.10.gif)

`10,1,10,1,10,1`  
![simpleLineOptions.width.values = 10,1,10,1,10,1](./images/simpleLineOptions.width.values.10.1.10.1.10.1.gif)

#### Color

##### Values (head to tail)

Single hex (RRGGBB without #, e.g. 000000 for black) or comma-separated list for a gradient from head to tail (e.g. 000000,FF0000,FFFFFF for black->red->white). Each color gets an equal share; repeat to widen. Invalid entries fall back to black.

`FF0000`  
![simpleLineOptions.color.values = FF0000](./images/simpleLineOptions.color.values.ff0000.gif)

`FF0000,FF7F00,FFFF00,7FFF00,00FF00,00FFFF,0000FF,4B0082,8B00FF`  
![simpleLineOptions.color.values = FF0000,FF7F00,FFFF00,7FFF00,00FF00,00FFFF,0000FF,4B0082,8B00FF](./images/simpleLineOptions.color.values.rainbow.gif)

##### Blend width

Percentage of each transition spent blending (0 = pure bands, 100 = full gradient). 50 with red,blue gives 25% hard red, 50% blend, 25% hard blue. Only applies with two or more colors.

`0`  
![simpleLineOptions.color.blend_width = 0](./images/simpleLineOptions.color.blend_width.0.png)  
![simpleLineOptions.color.blend_width = 0](./images/simpleLineOptions.color.blend_width.0.gif)  

`50`  
![simpleLineOptions.color.blend_width = 50](./images/simpleLineOptions.color.blend_width.50.png)  
![simpleLineOptions.color.blend_width = 50](./images/simpleLineOptions.color.blend_width.50.gif)  

`100`  
![simpleLineOptions.color.blend_width = 100](./images/simpleLineOptions.color.blend_width.100.png)  
![simpleLineOptions.color.blend_width = 100](./images/simpleLineOptions.color.blend_width.100.gif)  

#### Opacity

##### Values (head to tail)

Comma-separated opacity percentages (0-100) from head to tail (e.g. "100,0" for full fade, or "100,0,100" for a pulse). Each value gets an equal share; repeat to widen. Leave one value for uniform opacity.

`100`  
![simpleLineOptions.opacity.values = 100](./images/simpleLineOptions.opacity.values.100.gif)

`100,0`  
![simpleLineOptions.opacity.values = 100,0](./images/simpleLineOptions.opacity.values.100.0.gif)

`100,0,100`  
![simpleLineOptions.opacity.values = 100,0,100](./images/simpleLineOptions.opacity.values.100.0.100.gif)

#### Antialiasing

Smooth the trail edges.

`on`  
![simpleLineOptions.antialiasing = on](./images/simpleLineOptions.antialiasing.on.gif)

`off`  
![simpleLineOptions.antialiasing = off](./images/simpleLineOptions.antialiasing.off.gif)

### Cursor ghost options

Applies when Style is Cursor ghost.

#### Trail mode

How the copies disappear.

`ghostOptions.trail_mode: time_based`: each copy fades after the Tail duration  
![ghostOptions.trail_mode = time_based](./images/ghostOptions.trail_mode.time_based.gif)

`ghostOptions.trail_mode: size_based`: keeps a fixed number of copies even when the cursor stops  
![ghostOptions.trail_mode = size_based](./images/ghostOptions.trail_mode.size_based.gif)

#### Time based

Applies when Trail mode is Time based.

##### Tail duration

How long each cursor copy stays visible, in milliseconds. Minimum 20.

`100`  
![ghostOptions.timeBased.tail_duration = 100](./images/ghostOptions.timeBased.tail_duration.100.gif)

`300`  
![ghostOptions.timeBased.tail_duration = 300](./images/ghostOptions.timeBased.tail_duration.300.gif)

`1000`  
![ghostOptions.timeBased.tail_duration = 1000](./images/ghostOptions.timeBased.tail_duration.1000.gif)

#### Size based

Applies when Trail mode is Size based.

##### Copies

Number of cursor copies in the trail (Size based mode). Minimum 2, maximum 512.

`5`  
![ghostOptions.sizeBased.tail_size = 5](./images/ghostOptions.sizeBased.tail_size.5.gif)

`20`  
![ghostOptions.sizeBased.tail_size = 20](./images/ghostOptions.sizeBased.tail_size.20.gif)

`50`  
![ghostOptions.sizeBased.tail_size = 50](./images/ghostOptions.sizeBased.tail_size.50.gif)

##### Timeout

Milliseconds of inactivity before the trail starts fading (using the Time based tail duration). 0 = trail always visible.

`500`  
![ghostOptions.sizeBased.timeout = 500](./images/ghostOptions.sizeBased.timeout.500.gif)
 
#### Copy spacing

Extra distance in pixels added between cursor copies (0 = automatic, based on the copy count). Maximum 200.

`10`  
![ghostOptions.spacing = 10](./images/ghostOptions.spacing.10.gif)

`25`  
![ghostOptions.spacing = 25](./images/ghostOptions.spacing.25.gif)

#### Size

##### Values (head to tail)

Comma-separated size multipliers from head to tail (1 = same size, 0.8 = 80%, 2 = twice). Each value gets an equal share; repeat to widen (e.g. "1,0.5,1,0.5"). Avoid values above 1 (upscaled copies look pixelated); use the Windows cursor size setting to enlarge the cursor.

`1`  
![ghostOptions.size.values = 1](./images/ghostOptions.size.values.1.png)

`1,0`  
![ghostOptions.size.values = 1,0](./images/ghostOptions.size.values.1.0.png)

`1,0.2,1,0.2`  
![ghostOptions.size.values = 1,0.2,1,0.2](./images/ghostOptions.size.values.1.02.1.02.png)

#### Opacity

##### Values (head to tail)

Comma-separated opacity percentages (0-100) from head to tail (e.g. "100,0" for full fade). Each value gets an equal share; repeat to widen.

`100,0`  
![ghostOptions.opacity.values = 100,0](./images/ghostOptions.opacity.values.100.0.png)

`100,20,100,20`  
![ghostOptions.opacity.values = 100,20,100,20](./images/ghostOptions.opacity.values.100.20.100.20.png)

#### Color

##### Values (head to tail)

Leave empty to keep the cursor's own colors. Otherwise a single hex (RRGGBB without #) or comma-separated list for a gradient from head to tail; pixels matching the Replace color are recolored to this value (FFFFFF makes white copies). Each color gets an equal share; repeat to widen. Invalid entries fall back to black. A single color disables Blend width.

`(empty)`  
![ghostOptions.color.values = empty](./images/ghostOptions.color.values.empty.black.gif)  
![ghostOptions.color.values = empty](./images/ghostOptions.color.values.empty.pink.gif)  
![ghostOptions.color.values = empty](./images/ghostOptions.color.values.empty.green.gif)  

`ff0000`  
![ghostOptions.color.values = ff0000](./images/ghostOptions.color.values.ff0000.gif)

`ff00ff,00ffff,ffff00`  
![ghostOptions.color.values = ff00ff,00ffff,ffff00](./images/ghostOptions.color.values.ff00ff.00ffff.ffff00.gif)

##### Blend width

Percentage of each transition spent blending (0 = pure bands, 100 = full gradient). Only applies with two or more colors. (same principle as for the trail)

##### Replace

Only used when Color > Values is set.

###### Mode

Which cursor pixels to recolor.

`ghostOptions.color.replace.mode: auto`: the cursor's enclosed center color (ignoring the outline/contour; falls back to the largest area when nothing is enclosed)  
![ghostOptions.color.replace.mode = auto](./images/ghostOptions.color.replace.mode.auto.gif)

`ghostOptions.color.replace.mode: whole`: every non-transparent pixel  
![ghostOptions.color.replace.mode = whole](./images/ghostOptions.color.replace.mode.whole.gif)

`ghostOptions.color.replace.mode: custom`: the Custom color defined in the next setting (see below for examples) 

###### Custom color

Original cursor color to replace with the Values color (used when Mode is Custom). Set 000000 to recolor a black cursor body, or FFFFFF to recolor a white outline.

`ffffff`  
![ghostOptions.color.replace.mode = custom](./images/ghostOptions.color.replace.mode.custom.ffffff.gif)  
`f7bb0e`    
![ghostOptions.color.replace.mode = custom](./images/ghostOptions.color.replace.mode.custom.f7bb0e.gif)  
`000000`    
![ghostOptions.color.replace.mode = custom](./images/ghostOptions.color.replace.mode.custom.000000.gif)

### Enable/disable hotkey

#### Key

Press to toggle the trail on or off. Format: Modifier+Key (e.g. Ctrl+Alt+T). Modifiers: Ctrl, Alt, Shift, Win. At least one modifier is required. Leave empty to disable the hotkey.

#### Animation

Show a circle animation when the hotkey toggles the trail.    
![hotkeyOptions.animate = on](./images/hotkeyOptions.animate.gif)

The animation is a circle outline (2px, in the cursor's color, centered on the trail start and following the cursor): it grows and fades out when disabling, and shrinks and fades in when enabling.  

### Trail offset
Fine-tune the trail origin horizontally in pixels (auto-centered by default, 0 = no adjustment)

`0 0`  
![tail_offset.x = -10](./images/tail_offset.0.0.gif)

`15 15`  
![tail_offset.y = +10](./images/tail_offset.15.15.gif)

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

### Settings & blending

- `LoadSettings` uses `ReadStringSetting`, `ParseFloatList`, `SplitAndTrim`, and `ParseHexColor`, and precomputes color band boundaries (`settings.colorBandStart`/`colorBandEnd`) and opacity alphas (`settings.opacityValues`, stored as 0–1) so the hot path does no parsing or per-frame allocation. `LoadCommonTrailSettings(prefix)` reads the settings shared by both styles (trail mode, tail duration/size, timeout, opacity) from `ghostOptions` or `simpleLineOptions`, and `LoadColorSettings(prefix, defaultColor)` reads the per-style color gradient into `settings.activeColorsRGB` (and precomputes the ghost tint samples into `settings.ghostTints`); `TrailPointBudget`/`AutoPointSpacing` hold the shared point-count and spacing formulas.
- `GetBlendedColor`, `InterpolateValues`, and `InterpolateOpacity` are allocation-free; `Ease` applies the smoothstep easing curve.

### Cursor geometry

`UpdateCursorCenterOffset` caches, per `HCURSOR`: the bitmap-center offset (for the debug boxes), the visible-pixel-center offset (the line-style trail origin), the hotspot, the alpha-trimmed visible bounds, and the DPI scale. It is rebuilt only when the cursor handle changes. `ComputeCursorGeom` holds the shared computation; `GetCursorGeom` lazily computes geometry for any cursor handle still referenced by the trail (so the ghost style can draw older images after an image change). Ghost samples store the raw cursor hotspot, and the ghost renderer anchors each copy by its own image's hotspot, so a copy lands exactly where that cursor image was — independent of the render thread's offset refresh. The ghost style also builds and caches D2D bitmaps per `HCURSOR` via `EnsureCursorBitmap`, rendering the cursor with `DrawIconEx` at the on-screen pixel size (color + mask + anti-aliased alpha) so copies are blitted 1:1 without resampling. The `ghostOptions.color` gradient (when set) is baked into the cached pixels by sampling it at `kGhostTintSteps` ratios (one bitmap per sample; an empty color list yields a single untinted variant), and `GetCursorBitmap(hCursor, ratio)` selects the nearest variant for each copy. `color.replace.mode` picks the pixels to recolor: `whole` swaps every non-transparent pixel; `auto` splits each pixel between two reference colors — the replace color (the cursor's enclosed center color from `AnalyzeCursorColors`, falling back to the largest region when nothing is enclosed) and a keep color (the largest boundary/outline region) — swapping pixels closer to the replace color with a small softness band around the midpoint; `custom` replaces pixels whose color matches `color.replace.custom` (soft falloff so anti-aliased edges blend), leaving every other color untouched. Both caches are released with the render target.

### Lifecycle

- `WhTool_ModInit` — `LoadSettings()` then spawns `OverlayThreadProc`.
- `WhTool_ModSettingsChanged` — `LoadSettings()`, then posts `kMsgApplyHotkey` to the overlay window to re-register the hotkey on its thread.
- `WhTool_ModUninit` — signals the poll thread, kills the timer, and posts `WM_QUIT`.
- The overlay thread registers the `hotkeyOptions.key` setting (`ApplyHotkey`) right after creating the window and unregisters it before destroying the window. `WM_HOTKEY` flips `runtime.trailEnabled`, which suppresses sampling/rendering like the fullscreen-game path does, and (when `hotkeyOptions.animate` is on) calls `StartToggleEffect` to play the circle animation centered on the trail head (disable: grows + fades out, ease in; enable: shrinks + fades in, ease out; 400 ms, 2px outline, diameter 6× the cursor, colored by `GetCursorColor`'s ghost-`auto` pick, following the cursor).
- The `Wh_ModInit` / `Wh_ModAfterInit` / `Wh_ModUninit` block at the bottom of the file is Windhawk's tool-mod launcher boilerplate and should be left as-is.
