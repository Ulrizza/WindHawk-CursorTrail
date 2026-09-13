// ==WindhawkMod==
// @id              cursor-trail
// @name            Cursor trail
// @description     Cursor trail overlay with configurable styles (simple line)
// @version         0.13
// @author          Ulrizza
// @license         MIT
// @include         windhawk.exe
// @compilerOptions -ld2d1 -lole32 -lgdi32 -lshell32 -lwindowscodecs -lwinmm -lshcore
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Cursor trail

Draws a trail behind the mouse cursor that follows its movement.

## Style

- **Simple line** — a line whose width, color, and opacity can change
  along its length.

## Trail behavior

- **Trail mode** — choose how the trail disappears:
  - *Time based*: each part of the trail fades after a set duration.
  - *Size based*: the trail keeps a fixed length, even when the cursor stops.
- **Tail duration / Tail length** — how long (ms) or how far (px) the trail
  extends.
- **Timeout** (size based only) — how long the cursor must be still before
  the trail starts fading (0 = always visible).
- **Trail origin on cursor change** — what happens when the cursor image
  changes: *None* freezes the origin, *Immediate* snaps to the new cursor
  center, *Smooth* glides there.
- The trail fades out when the cursor is hidden (e.g. while typing), and
  rendering pauses over fullscreen games.

## Appearance

- **Width** — comma-separated stroke widths from head to tail, e.g. `2,1`
  for a tapered trail, or `10,1,10,1` for a pulsing one. Repeat a value to
  give it a bigger share (`2,2,2,2,1` = 80% at 2, 20% at 1).
- **Color** — a single hex color (`RRGGBB`) or a comma-separated list for a
  gradient from head to tail (e.g. `000000,FF0000,FFFFFF`).
- **Blend width** — how much each color transition blends: `0` for hard
  bands, `100` for a full gradient.
- **Interpolation** — curve used to blend between colors (*Linear*,
  *Smoothstep*, *Ease in*, *Ease out*).
- **Opacity** — comma-separated opacity percentages (0–100) from head to
  tail, e.g. `100,0` to fade out.
- **Antialiasing** — smooth or hard trail edges.

## Fine-tuning

- **Trail offset (X / Y)** — nudge the trail origin in pixels; it is
  centered on the cursor by default.
- **Debug: Show outline** — overlay boxes on the detected cursor and trail
  start to check alignment.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- style: simple_line
  $name: Style
  $description: Type of trail
  $options:
  - simple_line: Simple line
  - cursor_ghost: Cursor ghost
- simpleLineOptions:
  - trail_mode: "time_based"
    $name: Trail mode
    $description: Time based fades the trail over time; Size based keeps a fixed trail length even when the cursor stops
    $options:
    - time_based: Time based
    - size_based: Size based
  - antialiasing: "true"
    $name: Antialiasing
    $description: Smooth the trail edges
    $options:
    - "true": "True"
    - "false": "False"
  - trail_origin_on_cursor_change: "smooth"
    $name: Trail origin on cursor change
    $description: "What the trail origin does when the cursor image changes: None freezes it, Immediate snaps to the new cursor center, Smooth glides with an ease-in-out transition."
    $options:
    - none: None
    - immediate: Immediate
    - smooth: Smooth
  - timeBased:
    - tail_duration: 500
      $name: Tail duration
      $description: How long each trail segment stays visible, in milliseconds. Minimum 20.
    $name: Time based
  - sizeBased:
    - tail_size: 2000
      $name: Tail length
      $description: Maximum trail length in pixels. Minimum 20.
    - timeout: 2000
      $name: Timeout
      $description: Milliseconds of inactivity before the trail starts fading (using the Time based tail duration). 0 = trail always visible.
    $name: Size based
  - width:
    - values: "2,1"
      $name: Values
      $description: "Comma-separated stroke widths in pixels from head to tail (e.g. \"2,1\" for a tapered trail, or \"10,1,10,1\" for a pulsing trail). Each value gets an equal share; repeat to widen (e.g. \"2,2,2,2,1\" = 80%% at 2, 20%% at 1). Minimum 1."
    $name: Width
  - color:
    - values: "FF0000,FF7F00,FFFF00,7FFF00,00FF00,00FFFF,0000FF,4B0082,8B00FF"
      $name: Values
      $description: "Single hex (RRGGBB without #, e.g. 000000 for black) or comma-separated list for a gradient from head to tail (e.g. 000000,FF0000,FFFFFF for black->red->white). Each color gets an equal share; repeat to widen. Invalid entries are skipped."
    - blend_width: 100
      $name: Blend width
      $description: Percentage of each transition spent blending (0 = pure bands, 100 = full gradient). 50 with red,blue gives 25% hard red, 50% blend, 25% hard blue.
    - interpolation: "smoothstep"
      $name: Interpolation
      $description: Easing curve used to blend between colors
      $options:
      - linear: Linear
      - smoothstep: Smoothstep
      - ease_in: Ease in
      - ease_out: Ease out
    $name: Color
  - opacity:
    - values: "100,80"
      $name: Values
      $description: Comma-separated opacity percentages (0-100) from head to tail (e.g. "100,0" for full fade, or "100,0,100" for a pulse). Each value gets an equal share; repeat to widen. Leave one value for uniform opacity.
    $name: Opacity
  $name: Simple line options
- tail_offset:
  - x: 0
    $name: X
    $description: Fine-tune the trail origin horizontally in pixels (auto-centered by default, 0 = no adjustment)
  - y: 0
    $name: Y
    $description: Fine-tune the trail origin vertically in pixels (auto-centered by default, 0 = no adjustment)
  $name: Trail offset
- debug:
  - show_outline: false
    $name: Show outline
    $description: Draw a white box around the detected cursor bitmap, a red box around its visible (alpha-trimmed) pixels, and a blue + at the trail start, to verify the trail origin and cursor size.
  $name: Debug
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <d2d1.h>
#include <math.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <wincodec.h>
#include <mmsystem.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

// Position sample used by the polling thread and spatial decimation in the render loop.
struct Sample {
    POINT pos;
    DWORD t;  // Timestamp captured at sampling time (timeGetTime())
};

// Global state variables

// How the trail origin reacts to cursor image changes (Simple line only).
enum TrailOriginMode { ORIGIN_NONE, ORIGIN_IMMEDIATE, ORIGIN_SMOOTH };

// Easing curves used for color blending and interpolation.
enum ColorInterpolation { INTERP_LINEAR, INTERP_SMOOTHSTEP, INTERP_EASE_IN, INTERP_EASE_OUT };

struct Rgb { float r, g, b; };

// Parsed settings, written by LoadSettings and read by all threads.
struct Settings {
    int   tailOffsetX = 0, tailOffsetY = 0;
    int   tailDuration = 1000;
    bool  sizeBased = false;                          // trail mode: size_based vs time_based
    int   tailSize = 2000;
    DWORD sizeTimeout = 0;
    bool  antialiasing = true;
    bool  debugShowOutline = false;

    std::wstring    activeStyle = L"simple_line";
    std::wstring    trailOriginOnCursorChange = L"smooth";
    TrailOriginMode trailOriginMode = ORIGIN_SMOOTH;

    std::vector<float> simpleLineWidths;              // parsed width values, one per stop
    std::vector<Rgb>   simpleLineColorsRGB;           // pre-parsed for hot-path use
    int   colorBlendWidth = 0;
    ColorInterpolation colorInterp = INTERP_SMOOTHSTEP;
    float colorBlendHalf = 0.0f;
    std::vector<float> colorBandStart;                // precomputed pure-band boundaries
    std::vector<float> colorBandEnd;
    std::vector<float> simpleLineOpacityValues;       // parsed opacity alphas (0.0-1.0)
};

// Cursor geometry cache. The center/visual/frozen offsets are shared with the
// poll thread via offsetMutex; the debug dimensions below are render-thread-only.
struct CursorState {
    HCURSOR cachedCursor = NULL;
    POINT   centerOffset = { 0, 0 };  // hotspot → bitmap center (anchors debug boxes)
    POINT   visualOffset = { 0, 0 };  // hotspot → visible-pixel center (trail origin)
    POINT   frozenCursorOffset = { 0, 0 };  // snapshot of the trail origin at trail start
    std::mutex offsetMutex;                 // protects center/visual/frozen offsets (read by poll thread)

    int   bmWidth = 0, bmHeight = 0;        // bitmap dims + DPI scale for the debug boxes
    float dpiScaleX = 1.0f, dpiScaleY = 1.0f;
    bool  visibleValid = false;             // visible (alpha-trimmed) bounds
    int   visLeft = 0, visTop = 0, visRight = 0, visBottom = 0;
};

// Ease-in-out origin transition state (poll-thread-owned).
struct OriginTransition {
    float fromX = 0.0f, fromY = 0.0f;       // value at transition start
    POINT target = { 0, 0 };                // current target offset
    float smoothedOffsetX = 0.0f, smoothedOffsetY = 0.0f;
    DWORD startTime = 0;                    // time_based transition start
    float progressDist = 0.0f;              // size_based distance travelled
    bool  transitioning = false;
    bool  initialized = false;
    POINT lastCursorPos = { 0, 0 };         // for distance accumulation
    bool  lastCursorValid = false;
};

// Direct2D + backbuffer resources.
struct RenderResources {
    ID2D1Factory*         pD2DFactory = nullptr;
    ID2D1DCRenderTarget*  pDCRenderTarget = nullptr;
    ID2D1SolidColorBrush* pSimpleLineBrush = nullptr;
    ID2D1StrokeStyle*     pStrokeStyle = nullptr;
    // DEBUG brushes (temporary): white = bitmap bounds, red = visible pixels, blue = trail start.
    ID2D1SolidColorBrush* pDebugBrush = nullptr;
    ID2D1SolidColorBrush* pDebugBrushRed = nullptr;
    ID2D1SolidColorBrush* pDebugBrushBlue = nullptr;

    HDC     hdcMem = NULL;                  // cached backbuffer
    HBITMAP hBitmap = NULL;
    int     cachedVW = 0, cachedVH = 0;
};

// Runtime state: threads, window, history, atomics, timer, and frame state.
struct Runtime {
    HWND   overlayHwnd = NULL;
    HANDLE threadHandle = NULL;
    DWORD  overlayThreadId = 0;
    std::deque<Sample> history;
    std::mutex historyMutex;                  // protects history (poll + render threads)
    HANDLE pollThread = NULL;
    HANDLE pollStopEvent = NULL;
    std::atomic<bool> isGameRunning{false};   // set by render thread, read by poll thread
    std::atomic<bool> cursorHidden{false};    // set by render thread, read by poll thread
    std::atomic<bool> renderScheduled{false}; // set by MMTimerCallback, cleared by overlay thread
    int     sampleRate = 1;                   // polling interval in ms
    MMRESULT mmTimerId = 0;

    DWORD lastMovementTime = 0;               // poll-thread-owned
    bool  isFading = false;

    RECT  prevDirtyRect = { 0, 0, 0, 0 };     // render-thread-only frame state
    bool  hasPrevDirty = false;
    bool  needsClear = false;
    DWORD lastFullscreenCheck = 0;
};

Settings         settings;
CursorState      cursor;
OriginTransition origin;
RenderResources  render;
Runtime          runtime;

static std::vector<std::wstring> SplitAndTrim(const std::wstring& input) {
    std::vector<std::wstring> result;
    size_t start = 0;
    while (start <= input.size()) {
        size_t comma = input.find(L',', start);
        std::wstring token = (comma == std::wstring::npos)
            ? input.substr(start)
            : input.substr(start, comma - start);
        size_t first = token.find_first_not_of(L" \t");
        size_t last = token.find_last_not_of(L" \t");
        if (first != std::wstring::npos && last != std::wstring::npos) {
            token = token.substr(first, last - first + 1);
        } else {
            token = L"";
        }
        if (!token.empty()) {
            result.push_back(token);
        }
        if (comma == std::wstring::npos) break;
        start = comma + 1;
    }
    return result;
}

// Parses a hex color like L"RRGGBB" or L"#RRGGBB" into float components [0.0–1.0].
// Returns false on failure; r/g/b are left unchanged on success only.
static bool ParseHexColor(const std::wstring& hex, float& r, float& g, float& b) {
    std::wstring s = hex;
    if (!s.empty() && s[0] == L'#') s = s.substr(1);
    if (s.length() != 6) return false;
    auto hexVal = [](wchar_t c) -> int {
        if (c >= L'0' && c <= L'9') return c - L'0';
        if (c >= L'a' && c <= L'f') return c - L'a' + 10;
        if (c >= L'A' && c <= L'F') return c - L'A' + 10;
        return -1;
    };
    int r0 = hexVal(s[0]); int r1 = hexVal(s[1]);
    int g0 = hexVal(s[2]); int g1 = hexVal(s[3]);
    int b0 = hexVal(s[4]); int b1 = hexVal(s[5]);
    if (r0 < 0 || r1 < 0 || g0 < 0 || g1 < 0 || b0 < 0 || b1 < 0) return false;
    int rv = r0 * 16 + r1;
    int gv = g0 * 16 + g1;
    int bv = b0 * 16 + b1;
    r = rv / 255.0f;
    g = gv / 255.0f;
    b = bv / 255.0f;
    return true;
}

// Reads a string setting, returning def when the setting is missing or empty.
// Wh_FreeStringSetting is handled here; the returned copy is owned by the caller.
static std::wstring ReadStringSetting(const wchar_t* key, const std::wstring& def) {
    PCWSTR s = Wh_GetStringSetting(key);
    std::wstring r = (s && *s) ? std::wstring(s) : def;
    if (s) Wh_FreeStringSetting(s);
    return r;
}

// Parses a comma-separated list of floats into out. Each value is clamped to
// [minV, maxV]; invalid entries become onError. Falls back to defaultToken when
// the setting is empty.
static void ParseFloatList(const wchar_t* key, const std::wstring& defaultToken,
                           float minV, float maxV, float onError,
                           std::vector<float>& out) {
    out.clear();
    std::wstring raw = ReadStringSetting(key, L"");
    std::vector<std::wstring> tokens = raw.empty() ? std::vector<std::wstring>() : SplitAndTrim(raw);
    if (tokens.empty()) tokens.push_back(defaultToken);
    for (const auto& tok : tokens) {
        try {
            float v = std::stof(tok);
            if (v < minV) v = minV;
            if (v > maxV) v = maxV;
            out.push_back(v);
        } catch (...) {
            out.push_back(onError);
        }
    }
}

void LoadSettings() {
    settings.tailOffsetX = Wh_GetIntSetting(L"tail_offset.x");
    settings.tailOffsetY = Wh_GetIntSetting(L"tail_offset.y");

    settings.debugShowOutline = Wh_GetIntSetting(L"debug.show_outline") != 0;

    // Trail mode
    settings.sizeBased = ReadStringSetting(L"simpleLineOptions.trail_mode", L"time_based") == L"size_based";

    settings.antialiasing = ReadStringSetting(L"simpleLineOptions.antialiasing", L"true") != L"false";

    settings.trailOriginOnCursorChange = ReadStringSetting(L"simpleLineOptions.trail_origin_on_cursor_change", L"smooth");
    if (settings.trailOriginOnCursorChange != L"none" &&
        settings.trailOriginOnCursorChange != L"immediate" &&
        settings.trailOriginOnCursorChange != L"smooth") {
        settings.trailOriginOnCursorChange = L"smooth";
    }

    settings.tailDuration = Wh_GetIntSetting(L"simpleLineOptions.timeBased.tail_duration");
    settings.tailSize = Wh_GetIntSetting(L"simpleLineOptions.sizeBased.tail_size");
    settings.sizeTimeout = Wh_GetIntSetting(L"simpleLineOptions.sizeBased.timeout");

    settings.activeStyle = ReadStringSetting(L"style", L"simple_line");

    // RG-3: unknown style value → fallback to simple_line
    if (settings.activeStyle != L"simple_line" && settings.activeStyle != L"cursor_ghost") {
        settings.activeStyle = L"simple_line";
    }

    // Re-anchor the trail origin on cursor image changes.
    if (settings.trailOriginOnCursorChange == L"immediate") {
        settings.trailOriginMode = ORIGIN_IMMEDIATE;
    } else if (settings.trailOriginOnCursorChange == L"smooth") {
        settings.trailOriginMode = ORIGIN_SMOOTH;
    } else {
        settings.trailOriginMode = ORIGIN_NONE;
    }

    if (settings.tailDuration < 20) settings.tailDuration = 20;
    if (settings.tailSize < 20) settings.tailSize = 20;
    if (settings.sizeTimeout < 0) settings.sizeTimeout = 0;

    // Parse width values (min 1, no upper clamp)
    ParseFloatList(L"simpleLineOptions.width.values", L"1", 1.0f, 1e30f, 1.0f, settings.simpleLineWidths);

    // Parse opacity values (percentages 0-100), then convert to 0-1 alphas.
    ParseFloatList(L"simpleLineOptions.opacity.values", L"100", 0.0f, 100.0f, 100.0f, settings.simpleLineOpacityValues);
    for (float& v : settings.simpleLineOpacityValues) v /= 100.0f;

    // Parse color values
    settings.simpleLineColorsRGB.clear();
    {
        std::wstring colorSetting = ReadStringSetting(L"simpleLineOptions.color.values", L"000000");
        std::vector<std::wstring> colorTokens = SplitAndTrim(colorSetting);
        if (colorTokens.empty()) {
            colorTokens.push_back(L"000000");
        }
        for (const auto& hex : colorTokens) {
            Rgb rgb;
            if (!ParseHexColor(hex, rgb.r, rgb.g, rgb.b)) {
                rgb = { 0, 0, 0 };
            }
            settings.simpleLineColorsRGB.push_back(rgb);
        }
    }

    settings.colorBlendWidth = Wh_GetIntSetting(L"simpleLineOptions.color.blend_width");
    if (settings.colorBlendWidth < 0) settings.colorBlendWidth = 0;
    if (settings.colorBlendWidth > 100) settings.colorBlendWidth = 100;

    std::wstring interp = ReadStringSetting(L"simpleLineOptions.color.interpolation", L"smoothstep");
    if (interp == L"ease_in")      settings.colorInterp = INTERP_EASE_IN;
    else if (interp == L"ease_out") settings.colorInterp = INTERP_EASE_OUT;
    else if (interp == L"smoothstep") settings.colorInterp = INTERP_SMOOTHSTEP;
    else                            settings.colorInterp = INTERP_LINEAR;

    // Precompute pure-band boundaries for GetBlendedColor.
    settings.colorBlendHalf = (settings.colorBlendWidth / 100.0f) / 2.0f;
    settings.colorBandStart.clear();
    settings.colorBandEnd.clear();
    {
        size_t N = settings.simpleLineColorsRGB.size();
        settings.colorBandStart.reserve(N);
        settings.colorBandEnd.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            float start = (i == 0) ? 0.0f : (float)i / (float)N + settings.colorBlendHalf;
            float end = (i == N - 1) ? 1.0f : (float)(i + 1) / (float)N - settings.colorBlendHalf;
            settings.colorBandStart.push_back(start);
            settings.colorBandEnd.push_back(end);
        }
    }
}

static void FreeIconInfoBitmaps(ICONINFO& ii) {
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask)  DeleteObject(ii.hbmMask);
}

// Reads a DWORD from the registry. Returns 0 and sets found=false on failure.
static DWORD ReadRegDword(HKEY root, const wchar_t* subKey, const wchar_t* valueName, bool& found) {
    found = false;
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) return 0;
    DWORD data = 0, size = sizeof(data), type = 0;
    if (RegQueryValueExW(hKey, valueName, nullptr, &type, (BYTE*)&data, &size) == ERROR_SUCCESS &&
        type == REG_DWORD && size == sizeof(data)) {
        found = true;
    }
    RegCloseKey(hKey);
    return data;
}

// Returns the true on-screen size (physical pixels) of the cursor on the monitor
// under the pointer. The Windows cursor-size setting is stored in the registry
// as a DPI-independent base size (HKCU\Control Panel\Cursors\CursorBaseSize,
// default 32); the system scales it by the monitor DPI. SM_CXCURSOR/SM_CYCURSOR
// only report the nominal default and ignore the setting, so they are used only
// as a fallback.
static void GetActualCursorSize(int& cx, int& cy) {
    UINT dpiX = 96, dpiY = 96;
    POINT pt;
    if (GetCursorPos(&pt)) {
        HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        if (hMon) {
            GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
        }
    }

    bool found = false;
    DWORD base = ReadRegDword(HKEY_CURRENT_USER, L"Control Panel\\Cursors",
                              L"CursorBaseSize", found);
    if (found && base > 0) {
        cx = (int)((double)base * dpiX / 96.0 + 0.5);
        cy = (int)((double)base * dpiY / 96.0 + 0.5);
    } else {
        cx = GetSystemMetricsForDpi(SM_CXCURSOR, dpiX);
        cy = GetSystemMetricsForDpi(SM_CYCURSOR, dpiY);
    }
}

static void ResolveCursorBitmapDimensions(ICONINFO& ii, int& bmWidth, int& bmHeight, HBITMAP& hbmToUse) {
    bmWidth = 0; bmHeight = 0; hbmToUse = NULL;
    if (ii.hbmColor) {
        BITMAP bm = {};
        if (GetObject(ii.hbmColor, sizeof(bm), &bm)) {
            bmWidth = bm.bmWidth;
            bmHeight = bm.bmHeight;
            hbmToUse = ii.hbmColor;
        }
    }
    if (!hbmToUse) {
        BITMAP bm = {};
        if (ii.hbmMask && GetObject(ii.hbmMask, sizeof(bm), &bm)) {
            bmWidth = bm.bmWidth;
            bmHeight = bm.bmHeight / 2;
            hbmToUse = ii.hbmMask;
        }
    }
}

// Computes the visible (non-transparent) pixel bounds of a cursor HBITMAP by
// scanning its alpha channel (threshold 8 ignores faint anti-aliased edges).
// Bounds are in bitmap pixel coordinates (right/bottom exclusive). Uses WIC
// only, so it doesn't require a Direct2D render target.
static void ComputeVisibleBounds(HBITMAP hbm, int& left, int& top, int& right, int& bottom, bool& valid) {
    left = top = right = bottom = 0;
    valid = false;

    IWICImagingFactory* pWicFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pWicFactory));
    if (FAILED(hr) || !pWicFactory) return;

    IWICBitmap* pWicBitmap = nullptr;
    hr = pWicFactory->CreateBitmapFromHBITMAP(hbm, NULL,
        WICBitmapUsePremultipliedAlpha, &pWicBitmap);
    if (SUCCEEDED(hr) && pWicBitmap) {
        UINT w = 0, h = 0;
        pWicBitmap->GetSize(&w, &h);
        IWICFormatConverter* pConv = nullptr;
        if (SUCCEEDED(pWicFactory->CreateFormatConverter(&pConv)) && pConv) {
            if (SUCCEEDED(pConv->Initialize(pWicBitmap, GUID_WICPixelFormat32bppBGRA,
                    WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
                UINT stride = w * 4;
                std::vector<BYTE> pixels((size_t)stride * h);
                if (SUCCEEDED(pConv->CopyPixels(nullptr, stride,
                        (UINT)pixels.size(), pixels.data()))) {
                    int minX = (int)w, minY = (int)h, maxX = -1, maxY = -1;
                    for (UINT y = 0; y < h; ++y) {
                        const BYTE* row = pixels.data() + (size_t)y * stride;
                        for (UINT x = 0; x < w; ++x) {
                            if (row[x * 4 + 3] > 8) {
                                if ((int)x < minX) minX = (int)x;
                                if ((int)x > maxX) maxX = (int)x;
                                if ((int)y < minY) minY = (int)y;
                                if ((int)y > maxY) maxY = (int)y;
                            }
                        }
                    }
                    if (maxX >= minX && maxY >= minY) {
                        left = minX; top = minY; right = maxX + 1; bottom = maxY + 1;
                        valid = true;
                    }
                }
            }
            pConv->Release();
        }
        pWicBitmap->Release();
    }
    pWicFactory->Release();
}

// Compute the offsets from the cursor's hotspot to (a) the bitmap center, used
// to anchor the debug outline boxes, and (b) the visible-pixel center, used as
// the trail origin. Cached per-HCURSOR so we only do the work when the cursor
// shape actually changes.
void UpdateCursorCenterOffset() {
    std::lock_guard<std::mutex> lock(cursor.offsetMutex);

    CURSORINFO ci = { sizeof(CURSORINFO) };
    if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING) || !ci.hCursor) {
        runtime.cursorHidden.store(true);
        cursor.centerOffset = { 0, 0 };
        cursor.visualOffset = { 0, 0 };
        cursor.visibleValid = false;
        cursor.bmWidth = 0;
        cursor.bmHeight = 0;
        cursor.cachedCursor = NULL;
        return;
    }
    runtime.cursorHidden.store(false);

    if (ci.hCursor == cursor.cachedCursor) {
        return;  // same cursor as last frame, reuse cached offset
    }

    ICONINFO ii = { };
    if (!GetIconInfo(ci.hCursor, &ii)) {
        cursor.centerOffset = { 0, 0 };
        cursor.visualOffset = { 0, 0 };
        cursor.visibleValid = false;
        cursor.bmWidth = 0;
        cursor.bmHeight = 0;
        cursor.cachedCursor = NULL;
        return;
    }

    int bmWidth = 0, bmHeight = 0;
    HBITMAP hbmToUse = NULL;
    ResolveCursorBitmapDimensions(ii, bmWidth, bmHeight, hbmToUse);

    if (bmWidth > 0 && bmHeight > 0) {
        int actualX = 0, actualY = 0;
        GetActualCursorSize(actualX, actualY);
        float sx = (float)actualX / (float)bmWidth;
        float sy = (float)actualY / (float)bmHeight;

        // Cache the bitmap dimensions and DPI scale for the debug outline boxes.
        cursor.bmWidth = bmWidth;
        cursor.bmHeight = bmHeight;
        cursor.dpiScaleX = sx;
        cursor.dpiScaleY = sy;

        // Bitmap center (anchors the debug outline boxes).
        cursor.centerOffset.x = (int)(((bmWidth / 2.0f) - (int)ii.xHotspot) * sx + 0.5f);
        cursor.centerOffset.y = (int)(((bmHeight / 2.0f) - (int)ii.yHotspot) * sy + 0.5f);

        // Visible-pixel center (trail origin). Falls back to the bitmap center.
        ComputeVisibleBounds(hbmToUse, cursor.visLeft, cursor.visTop,
                             cursor.visRight, cursor.visBottom, cursor.visibleValid);
        float visCx = bmWidth / 2.0f;
        float visCy = bmHeight / 2.0f;
        if (cursor.visibleValid) {
            visCx = (cursor.visLeft + cursor.visRight) / 2.0f;
            visCy = (cursor.visTop + cursor.visBottom) / 2.0f;
        }
        cursor.visualOffset.x = (int)((visCx - (int)ii.xHotspot) * sx + 0.5f);
        cursor.visualOffset.y = (int)((visCy - (int)ii.yHotspot) * sy + 0.5f);
    } else {
        cursor.centerOffset = { 0, 0 };
        cursor.visualOffset = { 0, 0 };
        cursor.visibleValid = false;
        cursor.bmWidth = 0;
        cursor.bmHeight = 0;
    }

    FreeIconInfoBitmaps(ii);

    cursor.cachedCursor = ci.hCursor;
}

bool IsGameRunning() {
    HWND hwnd = GetForegroundWindow();
    
    if (!hwnd || hwnd == GetDesktopWindow()) {
        return false;
    }

    // Cache the desktop worker handles so we don't spam the Windows string table search literally 60 times a second
    static HWND s_hwndProgman = FindWindowW(L"Progman", NULL);
    static HWND s_hwndWorkerW = FindWindowW(L"WorkerW", NULL);
    
    if (hwnd == s_hwndProgman || hwnd == s_hwndWorkerW) {
        return false;
    }

    QUERY_USER_NOTIFICATION_STATE state;
    if (SUCCEEDED(SHQueryUserNotificationState(&state))) {
        if (state == QUNS_RUNNING_D3D_FULL_SCREEN) {
            return true;
        }
    }

    RECT rcApp;
    GetWindowRect(hwnd, &rcApp);
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(hMonitor, &mi)) {
        bool isFullscreen = (rcApp.left <= mi.rcMonitor.left &&
                             rcApp.top <= mi.rcMonitor.top &&
                             rcApp.right >= mi.rcMonitor.right &&
                             rcApp.bottom >= mi.rcMonitor.bottom);
        if (isFullscreen) {
            RECT rcClip;
            if (GetClipCursor(&rcClip)) {
                int vW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                int vH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
                if ((rcClip.right - rcClip.left) < vW || (rcClip.bottom - rcClip.top) < vH) {
                    return true;
                }
            }
            
            CURSORINFO ci = { sizeof(CURSORINFO) };
            if (GetCursorInfo(&ci) && ci.flags == 0) {
                return true;
            }
        }
    }
    return false;
}

// Applies an easing curve to t (expected in [0,1]).
static float Ease(float t, ColorInterpolation mode) {
    switch (mode) {
        case INTERP_EASE_IN:    return t * t;
        case INTERP_EASE_OUT:   return 1.0f - (1.0f - t) * (1.0f - t);
        case INTERP_SMOOTHSTEP: return t * t * (3.0f - 2.0f * t);
        case INTERP_LINEAR:
        default:                return t;
    }
}

// Rounds a float to the nearest LONG, away from zero.
static LONG RoundToLong(float v) {
    return (LONG)(v + (v >= 0.0f ? 0.5f : -0.5f));
}

// Blend model: each of the N-1 transitions gets a blend zone of width
// (blendWidth/100), centered on the boundary. Outside blend zones, pure
// colors; inside, the configured easing curve. With more than two colors the
// zones overlap, merging into a multi-color gradient. Reads the precomputed
// band boundaries from LoadSettings.
static void GetBlendedColor(float ratio, float& r, float& g, float& b) {
    const std::vector<Rgb>& colors = settings.simpleLineColorsRGB;
    r = 0.0f; g = 0.0f; b = 0.0f;
    if (colors.empty()) return;
    if (colors.size() == 1) {
        r = colors[0].r; g = colors[0].g; b = colors[0].b;
        return;
    }

    int N = (int)colors.size();
    float half = settings.colorBlendHalf;

    // Check pure bands first
    for (int i = 0; i < N; ++i) {
        if (settings.colorBandEnd[i] > settings.colorBandStart[i] &&
            ratio >= settings.colorBandStart[i] && ratio <= settings.colorBandEnd[i]) {
            r = colors[i].r; g = colors[i].g; b = colors[i].b;
            return;
        }
    }

    // Ratio falls in one or more blend zones — find which ones
    int firstZone = -1, lastZone = -1;
    for (int i = 0; i < N - 1; ++i) {
        float center = (float)(i + 1) / (float)N;
        float start = center - half;
        float end   = center + half;
        if (ratio >= start && ratio <= end) {
            if (firstZone < 0) firstZone = i;
            lastZone = i;
        }
    }

    // Fallback — shouldn't happen, but just in case
    if (firstZone < 0) {
        r = colors[0].r; g = colors[0].g; b = colors[0].b;
        return;
    }

    float zoneStart = (float)(firstZone + 1) / (float)N - half;
    float zoneEnd   = (float)(lastZone + 1) / (float)N + half;
    if (zoneStart < 0.0f) zoneStart = 0.0f;
    if (zoneEnd > 1.0f) zoneEnd = 1.0f;
    float span = zoneEnd - zoneStart;
    if (span <= 0.0f) {
        r = colors[firstZone].r; g = colors[firstZone].g; b = colors[firstZone].b;
        return;
    }
    float frac = (ratio - zoneStart) / span;

    frac = Ease(frac, settings.colorInterp);

    // Colors are evenly spaced within the blend region, so the index and
    // fraction are computed directly (no position array allocation).
    int numColors = lastZone - firstZone + 2;
    float scaled = frac * (float)(numColors - 1);
    size_t idx = (size_t)scaled;
    if (idx > (size_t)(numColors - 2)) idx = (size_t)(numColors - 2);
    float f = scaled - (float)idx;

    const auto& c0 = colors[firstZone + (int)idx];
    const auto& c1 = colors[firstZone + (int)idx + 1];
    r = c0.r + (c1.r - c0.r) * f;
    g = c0.g + (c1.g - c0.g) * f;
    b = c0.b + (c1.b - c0.b) * f;
}

// Interpolates a single float value across equal shares, using smoothstep
// (same as the color blend easing). Stops are evenly spaced, so the index and
// fraction are computed directly without building a position array.
static float InterpolateWidth(const std::vector<float>& values,
                              float ratio) {
    if (values.empty()) return 1.0f;
    if (values.size() == 1) return values[0];

    size_t n = values.size();
    if (ratio <= 0.0f) return values.front();
    if (ratio >= 1.0f) return values.back();

    float scaled = ratio * (float)(n - 1);
    size_t idx = (size_t)scaled;
    if (idx > n - 2) idx = n - 2;
    float frac = Ease(scaled - (float)idx, INTERP_SMOOTHSTEP);

    return values[idx] + (values[idx + 1] - values[idx]) * frac;
}

static float InterpolateOpacity(float ratio) {
    float alpha = InterpolateWidth(settings.simpleLineOpacityValues, ratio);
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    return alpha;
}




// Render the "Simple line" style: a thin polyline following the smoothed trail,
// with per-segment opacity fading from full opacity at the head (smoothed[0], nearest cursor)
// to transparent at the tail (smoothed.back()). Width, color and max opacity are user-configurable.
void RenderSimpleLineStyle(const std::vector<D2D1_POINT_2F>& smoothed) {
    if (smoothed.size() < 2) return;
    if (!render.pDCRenderTarget) return;

    if (!render.pSimpleLineBrush) {
        render.pDCRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1.0f), &render.pSimpleLineBrush);
        if (!render.pSimpleLineBrush) return;
    }
    if (!render.pStrokeStyle && render.pD2DFactory) {
        D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
            D2D1_LINE_JOIN_ROUND, 10.0f,
            D2D1_DASH_STYLE_SOLID, 0.0f);
        render.pD2DFactory->CreateStrokeStyle(&props, nullptr, 0, &render.pStrokeStyle);
    }

    size_t segCount = smoothed.size() - 1;
    for (size_t i = 0; i < segCount; ++i) {
        float ratio = (segCount > 1) ? (float)i / (float)(segCount - 1) : 0.0f;
        float alpha = InterpolateOpacity(ratio);
        float strokeWidth = InterpolateWidth(settings.simpleLineWidths, ratio);
        if (strokeWidth < 0.5f) strokeWidth = 0.5f;

        float gr, gg, gb;
        GetBlendedColor(ratio, gr, gg, gb);

        render.pSimpleLineBrush->SetColor(D2D1::ColorF(gr * alpha, gg * alpha, gb * alpha, alpha));
        render.pDCRenderTarget->DrawLine(smoothed[i], smoothed[i + 1], render.pSimpleLineBrush,
                                    strokeWidth, render.pStrokeStyle);
    }
}

// Releases the brushes and render target owned by the current render target.
// render.pStrokeStyle is factory-owned, so it survives target recreation.
static void ReleaseRenderTargetResources() {
    if (render.pSimpleLineBrush) { render.pSimpleLineBrush->Release(); render.pSimpleLineBrush = nullptr; }
    if (render.pDebugBrush) { render.pDebugBrush->Release(); render.pDebugBrush = nullptr; }
    if (render.pDebugBrushRed) { render.pDebugBrushRed->Release(); render.pDebugBrushRed = nullptr; }
    if (render.pDebugBrushBlue) { render.pDebugBrushBlue->Release(); render.pDebugBrushBlue = nullptr; }
    if (render.pDCRenderTarget) { render.pDCRenderTarget->Release(); render.pDCRenderTarget = nullptr; }
}

// Expands a bounding box to include the given rect, or initializes it if unset.
static void GrowBBox(RECT& bbox, bool& hasBBox, LONG l, LONG t, LONG r, LONG b) {
    if (hasBBox) {
        if (l < bbox.left) bbox.left = l;
        if (t < bbox.top) bbox.top = t;
        if (r > bbox.right) bbox.right = r;
        if (b > bbox.bottom) bbox.bottom = b;
    } else {
        bbox.left = l; bbox.top = t; bbox.right = r; bbox.bottom = b;
        hasBBox = true;
    }
}

// Time-based eviction: drop samples older than settings.tailDuration, then cap the
// total count. Caller must hold runtime.historyMutex.
static void EvictByTime(DWORD now) {
    while (!runtime.history.empty() && (now - runtime.history.back().t) > (DWORD)settings.tailDuration)
        runtime.history.pop_back();
    const size_t kMaxSamples = (size_t)(settings.tailDuration);
    while (runtime.history.size() > kMaxSamples)
        runtime.history.pop_back();
}

// High-frequency cursor polling thread.
// Runs at runtime.sampleRate ms intervals (default 1 ms), pushes sampled positions
// into runtime.history when the trail is active. All D2D operations remain on the
// overlay/render thread — this thread only touches runtime.history (under mutex),
// GetCursorPos, and the atomic flags.
DWORD WINAPI PollThreadProc(LPVOID) {
    // Match the overlay thread's DPI awareness so coordinate spaces agree.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Wait on the stop event with a runtime.sampleRate ms timeout to drive the loop.
    while (WaitForSingleObject(runtime.pollStopEvent, runtime.sampleRate) == WAIT_TIMEOUT) {
        // Respect the game-running flag set by SmearTimerProc.
        if (runtime.isGameRunning.load()) continue;

        POINT pt;
        if (!GetCursorPos(&pt)) continue;

        // Compute the canvas offset (virtual-screen origin).
        int vX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int vY = GetSystemMetrics(SM_YVIRTUALSCREEN);

        {
            std::lock_guard<std::mutex> lock(runtime.historyMutex);

            // === EVICTION — runs every tick, regardless of cursor movement ===
            // Must happen before the duplicate-skip so that old samples expire
            // even when the cursor is stationary (duplicate-skip would otherwise
            // continue before reaching eviction, freezing the trail).
            DWORD now = timeGetTime();
            if (settings.sizeBased) {
                if ((settings.sizeTimeout > 0 && runtime.lastMovementTime > 0 &&
                     now - runtime.lastMovementTime > settings.sizeTimeout) ||
                    runtime.cursorHidden.load()) {
                    if (!runtime.isFading) {
                        runtime.isFading = true;
                        size_t n = runtime.history.size();
                        if (n > 1) {
                            size_t idx = 0;
                            for (auto it = runtime.history.rbegin(); it != runtime.history.rend(); ++it, ++idx) {
                                it->t = now - settings.tailDuration + (DWORD)((float)idx / (float)(n - 1) * settings.tailDuration);
                            }
                        }
                    }
                    EvictByTime(now);
                } else {
                    runtime.isFading = false;
                    // Distance-based eviction: walk from head (newest)
                    // backwards, accumulating pixel distance. Pop
                    // everything past where cumulative > settings.tailSize.
                    // This makes trail length independent of mouse DPI
                    // and cursor speed.
                    double cumulative = 0.0;
                    for (size_t i = 1; i < runtime.history.size(); ++i) {
                        double dx = (double)runtime.history[i].pos.x - (double)runtime.history[i-1].pos.x;
                        double dy = (double)runtime.history[i].pos.y - (double)runtime.history[i-1].pos.y;
                        cumulative += sqrt(dx * dx + dy * dy);
                        if (cumulative > settings.tailSize) {
                            while (runtime.history.size() > i)
                                runtime.history.pop_back();
                            break;
                        }
                    }
                }
            } else {
                EvictByTime(now);
            }

            // === CURSOR HIDDEN — stop sampling so the trail fades out ===
            // Eviction above has already run, so the trail retracts over the
            // tail duration (size-based re-timestamped by the fade path). No
            // new samples are pushed until the cursor is shown again.
            if (runtime.cursorHidden.load()) {
                origin.lastCursorValid = false;
                continue;
            }

            // === TRAIL ORIGIN — choose the offset for this sample ===
            // Immediate/Smooth (Simple line only) follow the current cursor's
            // visual center so the trail head lands correctly after a cursor
            // image change (arrow → I-beam). None snapshots the offset when a
            // new trail starts so all samples share one coordinate space.
            POINT originOffset;
            if (settings.trailOriginMode != ORIGIN_SMOOTH) {
                // Reset smooth-transition state so re-entering smooth mode
                // snaps cleanly instead of accumulating a stale cursor jump.
                origin.initialized = false;
                origin.lastCursorValid = false;
            }
            if (settings.trailOriginMode == ORIGIN_IMMEDIATE) {
                std::lock_guard<std::mutex> offsetLock(cursor.offsetMutex);
                originOffset = cursor.visualOffset;
            } else if (settings.trailOriginMode == ORIGIN_SMOOTH) {
                POINT target;
                {
                    std::lock_guard<std::mutex> offsetLock(cursor.offsetMutex);
                    target = cursor.visualOffset;
                }

                // Accumulate raw cursor travel (screen px) for size_based.
                float dist = 0.0f;
                if (origin.lastCursorValid) {
                    float ddx = (float)(pt.x - origin.lastCursorPos.x);
                    float ddy = (float)(pt.y - origin.lastCursorPos.y);
                    dist = sqrtf(ddx * ddx + ddy * ddy);
                }
                origin.lastCursorPos = pt;
                origin.lastCursorValid = true;

                if (!origin.initialized) {
                    // Snap to the current offset on first use (no glide from 0,0).
                    origin.smoothedOffsetX = (float)target.x;
                    origin.smoothedOffsetY = (float)target.y;
                    origin.target = target;
                    origin.initialized = true;
                } else if (!origin.transitioning &&
                           (target.x != origin.target.x || target.y != origin.target.y)) {
                    // Target changed — start an ease-in-out transition from the
                    // current smoothed value.
                    origin.fromX = origin.smoothedOffsetX;
                    origin.fromY = origin.smoothedOffsetY;
                    origin.target = target;
                    origin.startTime = now;
                    origin.progressDist = 0.0f;
                    origin.transitioning = true;
                }

                // Advance progress: time-driven (time_based) or distance-driven
                // (size_based), over a third of the configured tail value.
                if (origin.transitioning) {
                    float p;
                    if (settings.sizeBased) {
                        origin.progressDist += dist;
                        float len = (float)(settings.tailSize / 3);
                        if (len < 1.0f) len = 1.0f;
                        p = origin.progressDist / len;
                    } else {
                        float len = (float)(settings.tailDuration / 3);
                        if (len < 1.0f) len = 1.0f;
                        p = (float)(now - origin.startTime) / len;
                    }
                    if (p > 1.0f) p = 1.0f;
                    float e = Ease(p, INTERP_SMOOTHSTEP);
                    origin.smoothedOffsetX = origin.fromX + (origin.target.x - origin.fromX) * e;
                    origin.smoothedOffsetY = origin.fromY + (origin.target.y - origin.fromY) * e;
                    if (p >= 1.0f) origin.transitioning = false;
                } else {
                    origin.smoothedOffsetX = (float)target.x;
                    origin.smoothedOffsetY = (float)target.y;
                }

                originOffset.x = RoundToLong(origin.smoothedOffsetX);
                originOffset.y = RoundToLong(origin.smoothedOffsetY);
            } else {
                std::lock_guard<std::mutex> offsetLock(cursor.offsetMutex);
                if (runtime.history.empty()) {
                    cursor.frozenCursorOffset = cursor.visualOffset;
                }
                originOffset = cursor.frozenCursorOffset;
            }

            POINT newPt = { pt.x + originOffset.x - vX,
                            pt.y + originOffset.y - vY };

            // === DUPLICATE-SKIP — only blocks push, not eviction ===
            // Eviction has already run above, so it is safe to continue here.
            if (!runtime.history.empty() &&
                runtime.history.front().pos.x == newPt.x &&
                runtime.history.front().pos.y == newPt.y) {
                // Cursor hasn't moved since last sample — skip push.
                continue;
            }

            // === PUSH ===
            Sample s;
            s.pos = newPt;
            s.t = now;
            runtime.history.push_front(s);
            runtime.lastMovementTime = now;
            runtime.isFading = false;
        }
    }
    return 0;
}

// Forward declaration: MMTimerCallback is defined later (before
// OverlayThreadProc). Forward-declare so the compiler knows the signature.
void CALLBACK MMTimerCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

// Allocates/recreates the backbuffer bitmap when the screen size changes.
// Rebuilding the bitmap invalidates the render target, so it is released here.
static void EnsureBackbuffer(HDC hdcScreen, int vW, int vH) {
    if (!render.hBitmap || render.cachedVW != vW || render.cachedVH != vH) {
        if (render.hBitmap) DeleteObject(render.hBitmap);
        if (render.hdcMem) DeleteDC(render.hdcMem);

        render.hdcMem = CreateCompatibleDC(hdcScreen);
        render.hBitmap = CreateCompatibleBitmap(hdcScreen, vW, vH);
        SelectObject(render.hdcMem, render.hBitmap);

        render.cachedVW = vW;
        render.cachedVH = vH;

        if (render.pDCRenderTarget) {
            ReleaseRenderTargetResources();
        }
    }
}

// Creates the Direct2D render target and its brushes if not already present.
static void EnsureRenderTarget() {
    if (!render.pDCRenderTarget && render.pD2DFactory) {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT
        );

        render.pD2DFactory->CreateDCRenderTarget(&props, &render.pDCRenderTarget);
        if (render.pDCRenderTarget) {
            render.pDCRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::White), &render.pDebugBrush);
            render.pDCRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::Red), &render.pDebugBrushRed);
            render.pDCRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::Blue), &render.pDebugBrushBlue);
        }
    }
}

// Snapshot runtime.history and build spatially-decimated trail points into smoothed.
static void BuildTrailPoints(const POINT& pt, int vX, int vY,
                             std::vector<D2D1_POINT_2F>& smoothed) {
    // Adaptive min distance: smaller kMinDist for longer trails so decimation
    // keeps enough waypoints. Scales inversely with kMaxPoints, clamped to [2, 6].
    size_t kMaxPoints = settings.sizeBased ? (size_t)settings.tailSize : (size_t)settings.tailDuration;
    if (kMaxPoints < 2) kMaxPoints = 2;
    float kMinDist = 6.0f * (10.0f / (float)kMaxPoints);
    if (kMinDist < 2.0f) kMinDist = 2.0f;
    if (kMinDist > 6.0f) kMinDist = 6.0f;

    smoothed.reserve(kMaxPoints);

    // The front of the deque is the most recent sample (captured by the poll
    // thread at ~1ms intervals with the cursor-center offset already applied).
    // Using runtime.history.front() as the head guarantees monotonic ordering.
    std::lock_guard<std::mutex> lock(runtime.historyMutex);
    if (runtime.history.empty()) {
        // No history yet — fall back to the render thread's cursor position.
        // Read the origin offset under cursor.offsetMutex (nested inside
        // runtime.historyMutex — consistent lock order everywhere).
        POINT originOffset;
        {
            std::lock_guard<std::mutex> offsetLock(cursor.offsetMutex);
            originOffset = (settings.trailOriginMode == ORIGIN_NONE)
                ? cursor.frozenCursorOffset
                : cursor.visualOffset;
        }
        POINT headPt = { pt.x + originOffset.x - vX,
                         pt.y + originOffset.y - vY };
        smoothed.push_back(D2D1::Point2F(
            (float)headPt.x + settings.tailOffsetX,
            (float)headPt.y + settings.tailOffsetY));
    } else {
        // Point 0: newest poll sample (front of deque).
        smoothed.push_back(D2D1::Point2F(
            (float)runtime.history.front().pos.x + settings.tailOffsetX,
            (float)runtime.history.front().pos.y + settings.tailOffsetY));

        // Spatial decimation: keep only points at least kMinDist pixels apart,
        // producing evenly-spaced waypoints for consistent Chaikin smoothing.
        D2D1_POINT_2F prev = smoothed[0];
        for (const auto& s : runtime.history) {
            if (smoothed.size() >= kMaxPoints) break;
            float sx = (float)s.pos.x + settings.tailOffsetX;
            float sy = (float)s.pos.y + settings.tailOffsetY;
            float dx = sx - prev.x;
            float dy = sy - prev.y;
            if (dx * dx + dy * dy >= kMinDist * kMinDist) {
                smoothed.push_back(D2D1::Point2F(sx, sy));
                prev = smoothed.back();
            }
        }
    }
}

// Chaikin subdivision: smooths corners by inserting intermediate points,
// roughly doubling count per iteration (2 iterations).
static void ChaikinSmooth(std::vector<D2D1_POINT_2F>& smoothed) {
    for (int iter = 0; iter < 2; ++iter) {
        if (smoothed.size() < 3) break;
        std::vector<D2D1_POINT_2F> next_s;
        next_s.reserve(smoothed.size() * 2);
        next_s.push_back(smoothed.front());
        for (size_t i = 0; i < smoothed.size() - 1; ++i) {
            D2D1_POINT_2F p0 = smoothed[i];
            D2D1_POINT_2F p1 = smoothed[i + 1];
            next_s.push_back(D2D1::Point2F(2.0f / 3.0f * p0.x + 1.0f / 3.0f * p1.x, 2.0f / 3.0f * p0.y + 1.0f / 3.0f * p1.y));
            next_s.push_back(D2D1::Point2F(1.0f / 3.0f * p0.x + 2.0f / 3.0f * p1.x, 1.0f / 3.0f * p0.y + 2.0f / 3.0f * p1.y));
        }
        next_s.push_back(smoothed.back());
        smoothed.swap(next_s);
    }
}

// Computes the trail's bounding box, expanded for stroke width / anti-aliasing.
static void ComputeTrailBBox(const std::vector<D2D1_POINT_2F>& smoothed, RECT& bbox) {
    float minX = smoothed[0].x, maxX = smoothed[0].x;
    float minY = smoothed[0].y, maxY = smoothed[0].y;
    for (const auto& p : smoothed) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }

    int margin = 32;
    if (!settings.simpleLineWidths.empty()) {
        float maxW = settings.simpleLineWidths[0];
        for (float w : settings.simpleLineWidths) {
            if (w > maxW) maxW = w;
        }
        margin = (int)maxW + 16;
    }
    bbox.left   = (LONG)minX - margin;
    bbox.top    = (LONG)minY - margin;
    bbox.right  = (LONG)maxX + margin;
    bbox.bottom = (LONG)maxY + margin;
}

// Dispatches to the active style renderer and updates the clear flag.
static void RenderTrail(const std::vector<D2D1_POINT_2F>& smoothed) {
    if (smoothed.size() >= 2) {
        if (settings.activeStyle == L"simple_line") {
            RenderSimpleLineStyle(smoothed);
        }
        // Future styles: add else-if branches here, e.g.
        // else if (settings.activeStyle == L"glow") { RenderGlowStyle(smoothed); }
        runtime.needsClear = true;
    } else {
        runtime.needsClear = false;
    }
}

// Draws the debug outline boxes and trail-start marker (when enabled).
static void DrawDebug(const POINT& pt, int vX, int vY,
                      const std::vector<D2D1_POINT_2F>& smoothed,
                      RECT& bbox, bool& hasBBox) {
    // White/red outline boxes around the detected cursor bitmap and its
    // visible (alpha-trimmed) pixels.
    if (settings.debugShowOutline && cursor.bmWidth > 0 && cursor.bmHeight > 0) {
        POINT centerOffset;
        {
            std::lock_guard<std::mutex> offsetLock(cursor.offsetMutex);
            centerOffset = cursor.centerOffset;
        }
        float boxW = cursor.bmWidth * cursor.dpiScaleX;
        float boxH = cursor.bmHeight * cursor.dpiScaleY;
        float boxCx = (float)(pt.x + centerOffset.x - vX);
        float boxCy = (float)(pt.y + centerOffset.y - vY);
        D2D1_RECT_F debugBox = D2D1::RectF(boxCx - boxW / 2.0f,
                                           boxCy - boxH / 2.0f,
                                           boxCx + boxW / 2.0f,
                                           boxCy + boxH / 2.0f);
        if (render.pDebugBrush) {
            render.pDCRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
            render.pDCRenderTarget->DrawRectangle(debugBox, render.pDebugBrush, 1.0f);
        }
        if (cursor.visibleValid && render.pDebugBrushRed) {
            float baseX = boxCx - boxW / 2.0f;
            float baseY = boxCy - boxH / 2.0f;
            D2D1_RECT_F visBox = D2D1::RectF(
                baseX + cursor.visLeft * cursor.dpiScaleX,
                baseY + cursor.visTop * cursor.dpiScaleY,
                baseX + cursor.visRight * cursor.dpiScaleX,
                baseY + cursor.visBottom * cursor.dpiScaleY);
            render.pDCRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
            render.pDCRenderTarget->DrawRectangle(visBox, render.pDebugBrushRed, 1.0f);
        }
        GrowBBox(bbox, hasBBox,
                 (LONG)debugBox.left - 1, (LONG)debugBox.top - 1,
                 (LONG)debugBox.right + 1, (LONG)debugBox.bottom + 1);
    }

    // Blue "+" marking the exact trail start (head point).
    if (settings.debugShowOutline && !smoothed.empty() && render.pDebugBrushBlue) {
        float hx = smoothed[0].x;
        float hy = smoothed[0].y;
        const float half = 5.0f;
        render.pDCRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        render.pDCRenderTarget->DrawLine(D2D1::Point2F(hx - half, hy),
                                    D2D1::Point2F(hx + half, hy), render.pDebugBrushBlue, 1.0f);
        render.pDCRenderTarget->DrawLine(D2D1::Point2F(hx, hy - half),
                                    D2D1::Point2F(hx, hy + half), render.pDebugBrushBlue, 1.0f);
        GrowBBox(bbox, hasBBox,
                 (LONG)hx - (LONG)half - 1, (LONG)hy - (LONG)half - 1,
                 (LONG)hx + (LONG)half + 1, (LONG)hy + (LONG)half + 1);
    }
}

// Blits the backbuffer to the overlay window via UpdateLayeredWindow, using a
// dirty rect when possible and falling back to full screen otherwise. Skips the
// blit entirely when there's nothing to show (prevents full-screen compositor
// updates every frame when the cursor is stationary).
static void BlitOverlay(HWND hwnd, HDC hdcScreen, int vX, int vY, int vW, int vH,
                        const RECT& curBBox, bool hasCurBBox) {
    if (!hasCurBBox && !runtime.hasPrevDirty && !runtime.needsClear) {
        ReleaseDC(NULL, hdcScreen);
        return;
    }

    // Dirty rect = union of current + previous bounding boxes, in backbuffer
    // coords (origin at 0,0 = virtual screen origin).
    RECT dirtyRect;
    bool useDirtyRect = false;

    if (hasCurBBox && runtime.hasPrevDirty) {
        dirtyRect.left   = (curBBox.left   < runtime.prevDirtyRect.left)   ? curBBox.left   : runtime.prevDirtyRect.left;
        dirtyRect.top    = (curBBox.top    < runtime.prevDirtyRect.top)    ? curBBox.top    : runtime.prevDirtyRect.top;
        dirtyRect.right  = (curBBox.right  > runtime.prevDirtyRect.right)  ? curBBox.right  : runtime.prevDirtyRect.right;
        dirtyRect.bottom = (curBBox.bottom > runtime.prevDirtyRect.bottom) ? curBBox.bottom : runtime.prevDirtyRect.bottom;
        useDirtyRect = true;
    } else if (hasCurBBox) {
        dirtyRect = curBBox;
        useDirtyRect = true;
    } else if (runtime.hasPrevDirty) {
        // No current trail, but previous frame had one — erase it.
        dirtyRect = runtime.prevDirtyRect;
        useDirtyRect = true;
    }

    if (useDirtyRect) {
        if (dirtyRect.left < 0) dirtyRect.left = 0;
        if (dirtyRect.top < 0) dirtyRect.top = 0;
        if (dirtyRect.right > vW) dirtyRect.right = vW;
        if (dirtyRect.bottom > vH) dirtyRect.bottom = vH;

        // 768x768 cap — if exceeded, fall back to full screen.
        int dirtyW = dirtyRect.right - dirtyRect.left;
        int dirtyH = dirtyRect.bottom - dirtyRect.top;
        if (dirtyW > 768 || dirtyH > 768 || dirtyW <= 0 || dirtyH <= 0) {
            useDirtyRect = false;
        }
    }

    // Update previous-frame tracking for the next tick.
    if (hasCurBBox) {
        runtime.prevDirtyRect = curBBox;
        runtime.hasPrevDirty = true;
    } else if (!runtime.needsClear) {
        runtime.prevDirtyRect = { 0, 0, 0, 0 };
        runtime.hasPrevDirty = false;
    }

    BLENDFUNCTION blend = { 0 };
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    if (useDirtyRect) {
        POINT ptPos = { vX + dirtyRect.left, vY + dirtyRect.top };
        SIZE sizeWnd = { dirtyRect.right - dirtyRect.left,
                         dirtyRect.bottom - dirtyRect.top };
        POINT ptSrc = { dirtyRect.left, dirtyRect.top };
        UpdateLayeredWindow(hwnd, hdcScreen, &ptPos, &sizeWnd,
                            render.hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
    } else {
        POINT ptPos = { vX, vY };
        SIZE sizeWnd = { vW, vH };
        POINT ptSrc = { 0, 0 };
        UpdateLayeredWindow(hwnd, hdcScreen, &ptPos, &sizeWnd,
                            render.hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
    }

    ReleaseDC(NULL, hdcScreen);
}

VOID CALLBACK SmearTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(idEvent);

    POINT pt;
    GetCursorPos(&pt);

    // Periodically refresh the game-running detection and publish the result
    // to the polling thread via the atomic flag.
    if (dwTime - runtime.lastFullscreenCheck > 500) {
        bool gameNow = IsGameRunning();
        runtime.isGameRunning.store(gameNow);
        runtime.lastFullscreenCheck = dwTime;
    }

    // Read local copies of the atomic flags for consistent use within this frame.
    bool isGameCached = runtime.isGameRunning.load();

    int vX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vH = GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1;

    if (isGameCached) {
        {
            std::lock_guard<std::mutex> lock(runtime.historyMutex);
            bool wasEmpty = runtime.history.empty();
            runtime.history.clear();
            if (wasEmpty && !runtime.needsClear) {
                return;
            }
        }
    } else {
        // No game running — update cursor appearance caches. The poll thread
        // owns trail-origin selection (frozen when runtime.history is empty, unless
        // settings.trailOriginMode is Immediate/Smooth) and sample accumulation.
        UpdateCursorCenterOffset();
    }

    // Snapshot the current history size to decide whether to draw.
    bool historyEmpty;
    {
        std::lock_guard<std::mutex> lock(runtime.historyMutex);
        historyEmpty = runtime.history.empty();
    }

    if (!historyEmpty || runtime.needsClear || (settings.debugShowOutline && cursor.bmWidth > 0 && cursor.bmHeight > 0)) {
        HDC hdcScreen = GetDC(NULL);

        EnsureBackbuffer(hdcScreen, vW, vH);
        EnsureRenderTarget();

        RECT curBBox = { 0, 0, 0, 0 };
        bool hasCurBBox = false;

        if (render.pDCRenderTarget) {
            RECT rc = { 0, 0, vW, vH };
            render.pDCRenderTarget->BindDC(render.hdcMem, &rc);

            render.pDCRenderTarget->BeginDraw();
            render.pDCRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
            render.pDCRenderTarget->SetAntialiasMode(settings.antialiasing
                ? D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
                : D2D1_ANTIALIAS_MODE_ALIASED);

            std::vector<D2D1_POINT_2F> smoothed;
            BuildTrailPoints(pt, vX, vY, smoothed);

            if (smoothed.size() >= 2) {
                ChaikinSmooth(smoothed);
                ComputeTrailBBox(smoothed, curBBox);
                hasCurBBox = true;
            }
            RenderTrail(smoothed);

            DrawDebug(pt, vX, vY, smoothed, curBBox, hasCurBBox);

            HRESULT hr = render.pDCRenderTarget->EndDraw();
            if (hr == D2DERR_RECREATE_TARGET) {
                ReleaseRenderTargetResources();
            }
        }

        BlitOverlay(hwnd, hdcScreen, vX, vY, vW, vH, curBBox, hasCurBBox);
    }
}

// Custom window proc for the overlay. Handles WM_TIMER (posted by the
// multimedia timer callback) by calling SmearTimerProc directly. All other
// messages go to DefWindowProc. This keeps all rendering on the overlay
// thread while using the multimedia timer for non-coalesced wakeups.
LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_TIMER) {
        // Clear before rendering so a frame that arrives while this one is still
        // in flight can queue exactly one more render (bounded, no backlog).
        runtime.renderScheduled.store(false);
        SmearTimerProc(hwnd, uMsg, wParam, GetTickCount());
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Multimedia timer callback — runs on a system-managed thread. Does NOT
// call any D2D/window APIs directly. Just PostMessages the overlay window
// to wake the message loop on the overlay thread, which then runs
// SmearTimerProc via OverlayWndProc.
void CALLBACK MMTimerCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    UNREFERENCED_PARAMETER(uTimerID);
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(dwUser);
    UNREFERENCED_PARAMETER(dw1);
    UNREFERENCED_PARAMETER(dw2);

    // Coalesce: only post if no render is already pending. PostMessage does not
    // coalesce like SetTimer, so an unthrottled post every 8ms builds an
    // unbounded WM_TIMER backlog whenever a frame runs long.
    if (runtime.overlayHwnd && !runtime.renderScheduled.exchange(true)) {
        if (!PostMessage(runtime.overlayHwnd, WM_TIMER, 1, 0)) {
            // Window is gone or queue failed; clear so future renders aren't
            // permanently blocked.
            runtime.renderScheduled.store(false);
        }
    }
}

DWORD WINAPI OverlayThreadProc(LPVOID lpParam) {
    UNREFERENCED_PARAMETER(lpParam);

    // Direct2D demands COM to be initialized on this thread before it will talk to us
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // Tell Windows we aren't a blurry legacy piece of shit so mixed-DPI monitors don't fuck up the math
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &render.pD2DFactory);

    HINSTANCE hInstance = GetModuleHandle(NULL);
    const wchar_t CLASS_NAME[] = L"SmearFrameOverlayClass";

    WNDCLASS wc = { };
    wc.lpfnWndProc = OverlayWndProc; 
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    int screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int screenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1;

    runtime.overlayHwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        CLASS_NAME,
        L"SmearOverlay",
        WS_POPUP,
        screenX, screenY, screenW, screenH,
        NULL, NULL, hInstance, NULL
    );

    if (!runtime.overlayHwnd) return 0;

    ShowWindow(runtime.overlayHwnd, SW_SHOWNA);

    // Start the high-frequency cursor polling thread.
    // The stop event is a manual-reset event, initially non-signalled.
    runtime.pollStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (runtime.pollStopEvent) {
        runtime.pollThread = CreateThread(NULL, 0, PollThreadProc, NULL, 0, NULL);
    }

    // Use a multimedia timer instead of SetTimer. Multimedia timers have ~1ms
    // resolution and are not coalesced like WM_TIMER, giving smoother animation
    // under load. The callback PostMessages the overlay window, keeping all
    // rendering on this thread.
    timeBeginPeriod(1);
    // Fixed render interval (8ms = ~125Hz). The render rate setting was removed
    // because it has no visible effect after decoupling sampling from rendering.
    // The poll thread samples at 1ms independently; this timer only controls
    // how often the overlay is redrawn.
    const int kRenderIntervalMs = 8;
    runtime.mmTimerId = timeSetEvent(kRenderIntervalMs, 1, MMTimerCallback, 0, TIME_PERIODIC);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Stop the polling thread gracefully before tearing down D2D resources.
    if (runtime.pollStopEvent) {
        SetEvent(runtime.pollStopEvent);
    }
    if (runtime.pollThread) {
        WaitForSingleObject(runtime.pollThread, 200);
        CloseHandle(runtime.pollThread);
        runtime.pollThread = NULL;
    }
    if (runtime.pollStopEvent) {
        CloseHandle(runtime.pollStopEvent);
        runtime.pollStopEvent = NULL;
    }

    // Clean up our massive GPU footprint before checking out
    ReleaseRenderTargetResources();
    if (render.pStrokeStyle) { render.pStrokeStyle->Release(); render.pStrokeStyle = nullptr; }
    if (render.pD2DFactory) { render.pD2DFactory->Release(); render.pD2DFactory = nullptr; }

    if (render.hBitmap) DeleteObject(render.hBitmap);
    if (render.hdcMem) DeleteDC(render.hdcMem);

    // Kill the multimedia timer if still running (may have been killed
    // already by WhTool_ModUninit) and restore default timer resolution.
    if (runtime.mmTimerId) {
        timeKillEvent(runtime.mmTimerId);
        runtime.mmTimerId = 0;
    }
    timeEndPeriod(1);

    DestroyWindow(runtime.overlayHwnd);
    UnregisterClass(CLASS_NAME, hInstance);

    CoUninitialize();
    return 0;
}

BOOL WhTool_ModInit() {
    LoadSettings();
    runtime.threadHandle = CreateThread(NULL, 0, OverlayThreadProc, NULL, 0, &runtime.overlayThreadId);
    return TRUE;
}

void WhTool_ModUninit() {
    // Signal the polling thread to stop. The overlay thread's cleanup block
    // will also signal it and wait, but signalling here first ensures the poll
    // thread begins shutting down before the overlay window's WM_QUIT is posted.
    if (runtime.pollStopEvent) {
        SetEvent(runtime.pollStopEvent);
    }

    // Kill the multimedia timer BEFORE posting WM_QUIT. The timer posts
    // WM_TIMER messages at 125Hz; if left running, they flood the message
    // queue and starve WM_QUIT, causing the unload to hang forever.
    if (runtime.mmTimerId) {
        timeKillEvent(runtime.mmTimerId);
        runtime.mmTimerId = 0;
        Sleep(20);  // Let any in-flight MMTimerCallback fire and complete
    }

    if (runtime.overlayThreadId) {
        PostThreadMessage(runtime.overlayThreadId, WM_QUIT, 0, 0);
    }
    if (runtime.threadHandle) {
        DWORD waitResult = WaitForSingleObject(runtime.threadHandle, 5000);
        if (waitResult == WAIT_TIMEOUT) {
            // Safety net: if the overlay thread didn't exit cleanly in 5s,
            // force-terminate to avoid hanging Windhawk's unload.
            TerminateThread(runtime.threadHandle, 0);
        }
        CloseHandle(runtime.threadHandle);
        runtime.threadHandle = NULL;
    }
}

void WhTool_ModSettingsChanged() {
    LoadSettings();
}

////////////////////////////////////////////////////////////////////////////////
// Windhawk tool mod implementation for mods which don't need to inject to other
// processes or hook other functions. Context:
// https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process
//
// The mod will load and run in a dedicated windhawk.exe process.
//
// Paste the code below as part of the mod code, and use these callbacks:
// * WhTool_ModInit
// * WhTool_ModSettingsChanged
// * WhTool_ModUninit
//
// Currently, other callbacks are not supported.

bool g_isToolModProcessLauncher;
HANDLE g_toolModProcessMutex;

void WINAPI EntryPoint_Hook() {
    Wh_Log(L">");
    ExitThread(0);
}

BOOL Wh_ModInit() {
    timeBeginPeriod(1);

    DWORD sessionId;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) &&
        sessionId == 0) {
        return FALSE;
    }

    bool isExcluded = false;
    bool isToolModProcess = false;
    bool isCurrentToolModProcess = false;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    if (!argv) {
        Wh_Log(L"CommandLineToArgvW failed");
        return FALSE;
    }

    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-service") == 0 ||
            wcscmp(argv[i], L"-service-start") == 0 ||
            wcscmp(argv[i], L"-service-stop") == 0) {
            isExcluded = true;
            break;
        }
    }

    for (int i = 1; i < argc - 1; i++) {
        if (wcscmp(argv[i], L"-tool-mod") == 0) {
            isToolModProcess = true;
            if (wcscmp(argv[i + 1], WH_MOD_ID) == 0) {
                isCurrentToolModProcess = true;
            }
            break;
        }
    }

    LocalFree(argv);

    if (isExcluded) {
        return FALSE;
    }

    if (isCurrentToolModProcess) {
        g_toolModProcessMutex =
            CreateMutex(nullptr, TRUE, L"windhawk-tool-mod_" WH_MOD_ID);
        if (!g_toolModProcessMutex) {
            Wh_Log(L"CreateMutex failed");
            ExitProcess(1);
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            Wh_Log(L"Tool mod already running (%s)", WH_MOD_ID);
            ExitProcess(1);
        }

        if (!WhTool_ModInit()) {
            ExitProcess(1);
        }

        IMAGE_DOS_HEADER* dosHeader =
            (IMAGE_DOS_HEADER*)GetModuleHandle(nullptr);
        IMAGE_NT_HEADERS* ntHeaders =
            (IMAGE_NT_HEADERS*)((BYTE*)dosHeader + dosHeader->e_lfanew);

        DWORD entryPointRVA = ntHeaders->OptionalHeader.AddressOfEntryPoint;
        void* entryPoint = (BYTE*)dosHeader + entryPointRVA;

        Wh_SetFunctionHook(entryPoint, (void*)EntryPoint_Hook, nullptr);
        return TRUE;
    }

    if (isToolModProcess) {
        return FALSE;
    }

    g_isToolModProcessLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!g_isToolModProcessLauncher) {
        return;
    }

    WCHAR currentProcessPath[MAX_PATH];
    switch (GetModuleFileName(nullptr, currentProcessPath,
                              ARRAYSIZE(currentProcessPath))) {
        case 0:
        case ARRAYSIZE(currentProcessPath):
            Wh_Log(L"GetModuleFileName failed");
            return;
    }

    WCHAR
    commandLine[MAX_PATH + 2 +
                (sizeof(L" -tool-mod \"" WH_MOD_ID "\"") / sizeof(WCHAR)) - 1];
    swprintf_s(commandLine, L"\"%s\" -tool-mod \"%s\"", currentProcessPath,
               WH_MOD_ID);

    HMODULE kernelModule = GetModuleHandle(L"kernelbase.dll");
    if (!kernelModule) {
        kernelModule = GetModuleHandle(L"kernel32.dll");
        if (!kernelModule) {
            Wh_Log(L"No kernelbase.dll/kernel32.dll");
            return;
        }
    }

    using CreateProcessInternalW_t = BOOL(WINAPI*)(
        HANDLE hUserToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes, WINBOOL bInheritHandles,
        DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
        LPSTARTUPINFOW lpStartupInfo,
        LPPROCESS_INFORMATION lpProcessInformation,
        PHANDLE hRestrictedUserToken);
    CreateProcessInternalW_t pCreateProcessInternalW =
        (CreateProcessInternalW_t)GetProcAddress(kernelModule,
                                                 "CreateProcessInternalW");
    if (!pCreateProcessInternalW) {
        Wh_Log(L"No CreateProcessInternalW");
        return;
    }

    STARTUPINFO si{
        .cb = sizeof(STARTUPINFO),
        .dwFlags = STARTF_FORCEOFFFEEDBACK,
    };
    PROCESS_INFORMATION pi;
    if (!pCreateProcessInternalW(nullptr, currentProcessPath, commandLine,
                                 nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS,
                                 nullptr, nullptr, &si, &pi, nullptr)) {
        Wh_Log(L"CreateProcess failed");
        return;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void Wh_ModSettingsChanged() {
    if (g_isToolModProcessLauncher) {
        return;
    }

    WhTool_ModSettingsChanged();
}

void Wh_ModUninit() {
    timeEndPeriod(1);

    if (g_isToolModProcessLauncher) {
        return;
    }

    WhTool_ModUninit();
    ExitProcess(0);
}