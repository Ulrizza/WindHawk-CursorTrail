// ==WindhawkMod==
// @id              cursor-trail
// @name            Cursor trail
// @description     Cursor trail overlay with configurable styles (simple line)
// @version         0.12
// @author          Ulrizza
// @license         MIT
// @include         windhawk.exe
// @compilerOptions -ld2d1 -lole32 -lgdi32 -lshell32 -lwindowscodecs -lwinmm -lshcore
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*...*/
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
HWND g_overlayHwnd = NULL;
HANDLE g_threadHandle = NULL;
DWORD g_overlayThreadId = 0;
std::deque<Sample> g_history;
std::mutex g_historyMutex;          // protects g_history (accessed by poll + render threads)
HANDLE g_pollThread = NULL;
HANDLE g_pollStopEvent = NULL;
std::atomic<bool> g_isGameRunning(false); // set by render thread, read by poll thread
std::atomic<bool> g_cursorHidden(false);  // set by render thread, read by poll thread
std::atomic<bool> g_renderScheduled(false); // set by MMTimerCallback, cleared by the overlay thread
int g_sampleRate = 1;               // polling interval in ms
// Direct2D globals
ID2D1Factory* g_pD2DFactory = nullptr;
ID2D1DCRenderTarget* g_pDCRenderTarget = nullptr;

// Cached backbuffer so we stop nuking the RAM every frame
HDC g_hdcMem = NULL;
HBITMAP g_hBitmap = NULL;
int g_cachedVW = 0;
int g_cachedVH = 0;

// Settings cache
int g_tailOffsetX = 0;
int g_tailOffsetY = 0;
int g_tailDuration = 1000;
bool g_sizeBased = false;                               // trail mode: size_based vs time_based
std::wstring g_trailOriginOnCursorChange = L"smooth";  // raw setting

// How the trail origin reacts to cursor image changes (Simple line only).
enum TrailOriginMode { ORIGIN_NONE, ORIGIN_IMMEDIATE, ORIGIN_SMOOTH };
TrailOriginMode g_trailOriginMode = ORIGIN_SMOOTH;

// Easing curves used for color blending and interpolation.
enum ColorInterpolation { INTERP_LINEAR, INTERP_SMOOTHSTEP, INTERP_EASE_IN, INTERP_EASE_OUT };

// Ease-in-out origin transition state (poll-thread owned).
float g_originFromX = 0.0f, g_originFromY = 0.0f;  // value at transition start
POINT g_originTarget = { 0, 0 };                    // current target offset
float g_smoothedOffsetX = 0.0f, g_smoothedOffsetY = 0.0f;
DWORD g_originStartTime = 0;                        // time_based transition start
float g_originProgressDist = 0.0f;                  // size_based distance travelled
bool g_originTransitioning = false;
bool g_originInitialized = false;
POINT g_lastOriginCursorPos = { 0, 0 };             // for distance accumulation
bool g_lastOriginCursorValid = false;

int g_tailSize = 2000;
bool g_antialiasing = true;
bool g_debugShowOutline = false;

DWORD g_sizeTimeout = 0;
DWORD g_lastMovementTime = 0;
bool g_isFading = false;

// Active rendering style (copied from Wh_GetStringSetting, freed immediately)
std::wstring g_activeStyle = L"simple_line";

// Simple line style settings
std::vector<float> g_simpleLineWidths;       // parsed width values, one per stop

struct Rgb { float r, g, b; };
std::vector<Rgb> g_simpleLineColorsRGB;         // pre-parsed for hot-path use

int g_colorBlendWidth = 0;
ColorInterpolation g_colorInterp = INTERP_SMOOTHSTEP;
// Precomputed pure-band boundaries for GetBlendedColor (aligned with
// g_simpleLineColorsRGB). Built in LoadSettings.
float g_colorBlendHalf = 0.0f;
std::vector<float> g_colorBandStart;
std::vector<float> g_colorBandEnd;

std::vector<float> g_simpleLineOpacityValues; // parsed opacity alphas (0.0-1.0)

ID2D1SolidColorBrush* g_pSimpleLineBrush = nullptr;
ID2D1StrokeStyle* g_pStrokeStyle = nullptr;

// DEBUG: white 1px box around the detected cursor bitmap (temporary).
ID2D1SolidColorBrush* g_pDebugBrush = nullptr;
// DEBUG: red 1px box around the alpha-trimmed (visible) cursor pixels (temporary).
ID2D1SolidColorBrush* g_pDebugBrushRed = nullptr;
// DEBUG: blue "+" marking the exact trail start point (temporary).
ID2D1SolidColorBrush* g_pDebugBrushBlue = nullptr;

// Cursor visual-center cache (avoids re-querying GetIconInfo every frame
// when the cursor handle hasn't changed)
HCURSOR g_cachedCursor = NULL;
POINT g_cursorCenterOffset = { 0, 0 };  // added to hotspot to reach bitmap center (anchors debug boxes)
POINT g_cursorVisualOffset = { 0, 0 };  // added to hotspot to reach visible-pixel center (trail origin)

// Frozen trail-origin offset, snapshotted by the poll thread when a new
// trail starts (g_history is empty and a new sample is about to be pushed).
// All samples in a trail share the same coordinate space, even if the cursor
// shape changes mid-trail (e.g. arrow → I-beam). g_cursorVisualOffset is
// updated by the render thread; g_frozenCursorOffset is read and written by
// the poll thread. Unused when g_trailOriginMode is Immediate or Smooth
// (Simple line), in which case each sample follows the live visual offset.
POINT g_frozenCursorOffset = { 0, 0 };

std::mutex g_offsetMutex;  // protects g_cursorCenterOffset, g_cursorVisualOffset and g_frozenCursorOffset
                            // (read by the poll thread). Does NOT protect the debug-dimension
                            // globals below — those are render-thread-only.
                            // lock order: g_historyMutex (if needed) THEN g_offsetMutex


// Cursor bitmap dimensions and DPI scale, cached per-HCURSOR by
// UpdateCursorCenterOffset. Used by the debug outline boxes.
// Render-thread-only: written by UpdateCursorCenterOffset and read by
// SmearTimerProc, both on the overlay/render thread (no lock required).
int g_cursorBmWidth = 0;
int g_cursorBmHeight = 0;
float g_cursorDpiScaleX = 1.0f;
float g_cursorDpiScaleY = 1.0f;

// DEBUG: visible (alpha-trimmed) bounds of the cursor bitmap, in bitmap coords
// (right/bottom exclusive). Temporary, used for the red debug box.
// Render-thread-only (same ownership as the dimensions above).
bool g_cursorVisibleValid = false;
int g_cursorVisLeft = 0, g_cursorVisTop = 0, g_cursorVisRight = 0, g_cursorVisBottom = 0;

// Multimedia timer handle for the render loop. Replaces SetTimer/WM_TIMER
// for smoother, non-coalesced wakeups. The timer callback runs on a system
// thread and just PostMessages the overlay window — all rendering stays on
// the overlay thread.
MMRESULT g_mmTimerId = 0;

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
    g_tailOffsetX = Wh_GetIntSetting(L"tail_offset.x");
    g_tailOffsetY = Wh_GetIntSetting(L"tail_offset.y");

    g_debugShowOutline = Wh_GetIntSetting(L"debug.show_outline") != 0;

    // Trail mode
    g_sizeBased = ReadStringSetting(L"simpleLineOptions.trail_mode", L"time_based") == L"size_based";

    g_antialiasing = ReadStringSetting(L"simpleLineOptions.antialiasing", L"true") != L"false";

    g_trailOriginOnCursorChange = ReadStringSetting(L"simpleLineOptions.trail_origin_on_cursor_change", L"smooth");
    if (g_trailOriginOnCursorChange != L"none" &&
        g_trailOriginOnCursorChange != L"immediate" &&
        g_trailOriginOnCursorChange != L"smooth") {
        g_trailOriginOnCursorChange = L"smooth";
    }

    g_tailDuration = Wh_GetIntSetting(L"simpleLineOptions.timeBased.tail_duration");
    g_tailSize = Wh_GetIntSetting(L"simpleLineOptions.sizeBased.tail_size");
    g_sizeTimeout = Wh_GetIntSetting(L"simpleLineOptions.sizeBased.timeout");

    g_activeStyle = ReadStringSetting(L"style", L"simple_line");

    // RG-3: unknown style value → fallback to simple_line
    if (g_activeStyle != L"simple_line" && g_activeStyle != L"cursor_ghost") {
        g_activeStyle = L"simple_line";
    }

    // Re-anchor the trail origin on cursor image changes.
    if (g_trailOriginOnCursorChange == L"immediate") {
        g_trailOriginMode = ORIGIN_IMMEDIATE;
    } else if (g_trailOriginOnCursorChange == L"smooth") {
        g_trailOriginMode = ORIGIN_SMOOTH;
    } else {
        g_trailOriginMode = ORIGIN_NONE;
    }

    if (g_tailDuration < 20) g_tailDuration = 20;
    if (g_tailSize < 20) g_tailSize = 20;
    if (g_sizeTimeout < 0) g_sizeTimeout = 0;

    // Parse width values (min 1, no upper clamp)
    ParseFloatList(L"simpleLineOptions.width.values", L"1", 1.0f, 1e30f, 1.0f, g_simpleLineWidths);

    // Parse opacity values (percentages 0-100), then convert to 0-1 alphas.
    ParseFloatList(L"simpleLineOptions.opacity.values", L"100", 0.0f, 100.0f, 100.0f, g_simpleLineOpacityValues);
    for (float& v : g_simpleLineOpacityValues) v /= 100.0f;

    // Parse color values
    g_simpleLineColorsRGB.clear();
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
            g_simpleLineColorsRGB.push_back(rgb);
        }
    }

    g_colorBlendWidth = Wh_GetIntSetting(L"simpleLineOptions.color.blend_width");
    if (g_colorBlendWidth < 0) g_colorBlendWidth = 0;
    if (g_colorBlendWidth > 100) g_colorBlendWidth = 100;

    std::wstring interp = ReadStringSetting(L"simpleLineOptions.color.interpolation", L"smoothstep");
    if (interp == L"ease_in")      g_colorInterp = INTERP_EASE_IN;
    else if (interp == L"ease_out") g_colorInterp = INTERP_EASE_OUT;
    else if (interp == L"smoothstep") g_colorInterp = INTERP_SMOOTHSTEP;
    else                            g_colorInterp = INTERP_LINEAR;

    // Precompute pure-band boundaries for GetBlendedColor.
    g_colorBlendHalf = (g_colorBlendWidth / 100.0f) / 2.0f;
    g_colorBandStart.clear();
    g_colorBandEnd.clear();
    {
        size_t N = g_simpleLineColorsRGB.size();
        g_colorBandStart.reserve(N);
        g_colorBandEnd.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            float start = (i == 0) ? 0.0f : (float)i / (float)N + g_colorBlendHalf;
            float end = (i == N - 1) ? 1.0f : (float)(i + 1) / (float)N - g_colorBlendHalf;
            g_colorBandStart.push_back(start);
            g_colorBandEnd.push_back(end);
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
    std::lock_guard<std::mutex> lock(g_offsetMutex);

    CURSORINFO ci = { sizeof(CURSORINFO) };
    if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING) || !ci.hCursor) {
        g_cursorHidden.store(true);
        g_cursorCenterOffset = { 0, 0 };
        g_cursorVisualOffset = { 0, 0 };
        g_cursorVisibleValid = false;
        g_cursorBmWidth = 0;
        g_cursorBmHeight = 0;
        g_cachedCursor = NULL;
        return;
    }
    g_cursorHidden.store(false);

    if (ci.hCursor == g_cachedCursor) {
        return;  // same cursor as last frame, reuse cached offset
    }

    ICONINFO ii = { };
    if (!GetIconInfo(ci.hCursor, &ii)) {
        g_cursorCenterOffset = { 0, 0 };
        g_cursorVisualOffset = { 0, 0 };
        g_cursorVisibleValid = false;
        g_cursorBmWidth = 0;
        g_cursorBmHeight = 0;
        g_cachedCursor = NULL;
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
        g_cursorBmWidth = bmWidth;
        g_cursorBmHeight = bmHeight;
        g_cursorDpiScaleX = sx;
        g_cursorDpiScaleY = sy;

        // Bitmap center (anchors the debug outline boxes).
        g_cursorCenterOffset.x = (int)(((bmWidth / 2.0f) - (int)ii.xHotspot) * sx + 0.5f);
        g_cursorCenterOffset.y = (int)(((bmHeight / 2.0f) - (int)ii.yHotspot) * sy + 0.5f);

        // Visible-pixel center (trail origin). Falls back to the bitmap center.
        ComputeVisibleBounds(hbmToUse, g_cursorVisLeft, g_cursorVisTop,
                             g_cursorVisRight, g_cursorVisBottom, g_cursorVisibleValid);
        float visCx = bmWidth / 2.0f;
        float visCy = bmHeight / 2.0f;
        if (g_cursorVisibleValid) {
            visCx = (g_cursorVisLeft + g_cursorVisRight) / 2.0f;
            visCy = (g_cursorVisTop + g_cursorVisBottom) / 2.0f;
        }
        g_cursorVisualOffset.x = (int)((visCx - (int)ii.xHotspot) * sx + 0.5f);
        g_cursorVisualOffset.y = (int)((visCy - (int)ii.yHotspot) * sy + 0.5f);
    } else {
        g_cursorCenterOffset = { 0, 0 };
        g_cursorVisualOffset = { 0, 0 };
        g_cursorVisibleValid = false;
        g_cursorBmWidth = 0;
        g_cursorBmHeight = 0;
    }

    FreeIconInfoBitmaps(ii);

    g_cachedCursor = ci.hCursor;
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
    const std::vector<Rgb>& colors = g_simpleLineColorsRGB;
    r = 0.0f; g = 0.0f; b = 0.0f;
    if (colors.empty()) return;
    if (colors.size() == 1) {
        r = colors[0].r; g = colors[0].g; b = colors[0].b;
        return;
    }

    int N = (int)colors.size();
    float half = g_colorBlendHalf;

    // Check pure bands first
    for (int i = 0; i < N; ++i) {
        if (g_colorBandEnd[i] > g_colorBandStart[i] &&
            ratio >= g_colorBandStart[i] && ratio <= g_colorBandEnd[i]) {
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

    frac = Ease(frac, g_colorInterp);

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
    float alpha = InterpolateWidth(g_simpleLineOpacityValues, ratio);
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    return alpha;
}




// Render the "Simple line" style: a thin polyline following the smoothed trail,
// with per-segment opacity fading from full opacity at the head (smoothed[0], nearest cursor)
// to transparent at the tail (smoothed.back()). Width, color and max opacity are user-configurable.
void RenderSimpleLineStyle(const std::vector<D2D1_POINT_2F>& smoothed) {
    if (smoothed.size() < 2) return;
    if (!g_pDCRenderTarget) return;

    if (!g_pSimpleLineBrush) {
        g_pDCRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1.0f), &g_pSimpleLineBrush);
        if (!g_pSimpleLineBrush) return;
    }
    if (!g_pStrokeStyle && g_pD2DFactory) {
        D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
            D2D1_LINE_JOIN_ROUND, 10.0f,
            D2D1_DASH_STYLE_SOLID, 0.0f);
        g_pD2DFactory->CreateStrokeStyle(&props, nullptr, 0, &g_pStrokeStyle);
    }

    size_t segCount = smoothed.size() - 1;
    for (size_t i = 0; i < segCount; ++i) {
        float ratio = (segCount > 1) ? (float)i / (float)(segCount - 1) : 0.0f;
        float alpha = InterpolateOpacity(ratio);
        float strokeWidth = InterpolateWidth(g_simpleLineWidths, ratio);
        if (strokeWidth < 0.5f) strokeWidth = 0.5f;

        float gr, gg, gb;
        GetBlendedColor(ratio, gr, gg, gb);

        g_pSimpleLineBrush->SetColor(D2D1::ColorF(gr * alpha, gg * alpha, gb * alpha, alpha));
        g_pDCRenderTarget->DrawLine(smoothed[i], smoothed[i + 1], g_pSimpleLineBrush,
                                    strokeWidth, g_pStrokeStyle);
    }
}

// Time-based eviction: drop samples older than g_tailDuration, then cap the
// total count. Caller must hold g_historyMutex.
static void EvictByTime(DWORD now) {
    while (!g_history.empty() && (now - g_history.back().t) > (DWORD)g_tailDuration)
        g_history.pop_back();
    const size_t kMaxSamples = (size_t)(g_tailDuration);
    while (g_history.size() > kMaxSamples)
        g_history.pop_back();
}

// High-frequency cursor polling thread.
// Runs at g_sampleRate ms intervals (default 1 ms), pushes sampled positions
// into g_history when the trail is active. All D2D operations remain on the
// overlay/render thread — this thread only touches g_history (under mutex),
// GetCursorPos, and the atomic flags.
DWORD WINAPI PollThreadProc(LPVOID) {
    // Match the overlay thread's DPI awareness so coordinate spaces agree.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Wait on the stop event with a g_sampleRate ms timeout to drive the loop.
    while (WaitForSingleObject(g_pollStopEvent, g_sampleRate) == WAIT_TIMEOUT) {
        // Respect the game-running flag set by SmearTimerProc.
        if (g_isGameRunning.load()) continue;

        POINT pt;
        if (!GetCursorPos(&pt)) continue;

        // Compute the canvas offset (virtual-screen origin).
        int vX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int vY = GetSystemMetrics(SM_YVIRTUALSCREEN);

        {
            std::lock_guard<std::mutex> lock(g_historyMutex);

            // === EVICTION — runs every tick, regardless of cursor movement ===
            // Must happen before the duplicate-skip so that old samples expire
            // even when the cursor is stationary (duplicate-skip would otherwise
            // continue before reaching eviction, freezing the trail).
            DWORD now = timeGetTime();
            if (g_sizeBased) {
                if ((g_sizeTimeout > 0 && g_lastMovementTime > 0 &&
                     now - g_lastMovementTime > g_sizeTimeout) ||
                    g_cursorHidden.load()) {
                    if (!g_isFading) {
                        g_isFading = true;
                        size_t n = g_history.size();
                        if (n > 1) {
                            size_t idx = 0;
                            for (auto it = g_history.rbegin(); it != g_history.rend(); ++it, ++idx) {
                                it->t = now - g_tailDuration + (DWORD)((float)idx / (float)(n - 1) * g_tailDuration);
                            }
                        }
                    }
                    EvictByTime(now);
                } else {
                    g_isFading = false;
                    // Distance-based eviction: walk from head (newest)
                    // backwards, accumulating pixel distance. Pop
                    // everything past where cumulative > g_tailSize.
                    // This makes trail length independent of mouse DPI
                    // and cursor speed.
                    double cumulative = 0.0;
                    for (size_t i = 1; i < g_history.size(); ++i) {
                        double dx = (double)g_history[i].pos.x - (double)g_history[i-1].pos.x;
                        double dy = (double)g_history[i].pos.y - (double)g_history[i-1].pos.y;
                        cumulative += sqrt(dx * dx + dy * dy);
                        if (cumulative > g_tailSize) {
                            while (g_history.size() > i)
                                g_history.pop_back();
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
            if (g_cursorHidden.load()) {
                g_lastOriginCursorValid = false;
                continue;
            }

            // === TRAIL ORIGIN — choose the offset for this sample ===
            // Immediate/Smooth (Simple line only) follow the current cursor's
            // visual center so the trail head lands correctly after a cursor
            // image change (arrow → I-beam). None snapshots the offset when a
            // new trail starts so all samples share one coordinate space.
            POINT originOffset;
            if (g_trailOriginMode != ORIGIN_SMOOTH) {
                // Reset smooth-transition state so re-entering smooth mode
                // snaps cleanly instead of accumulating a stale cursor jump.
                g_originInitialized = false;
                g_lastOriginCursorValid = false;
            }
            if (g_trailOriginMode == ORIGIN_IMMEDIATE) {
                std::lock_guard<std::mutex> offsetLock(g_offsetMutex);
                originOffset = g_cursorVisualOffset;
            } else if (g_trailOriginMode == ORIGIN_SMOOTH) {
                POINT target;
                {
                    std::lock_guard<std::mutex> offsetLock(g_offsetMutex);
                    target = g_cursorVisualOffset;
                }

                // Accumulate raw cursor travel (screen px) for size_based.
                float dist = 0.0f;
                if (g_lastOriginCursorValid) {
                    float ddx = (float)(pt.x - g_lastOriginCursorPos.x);
                    float ddy = (float)(pt.y - g_lastOriginCursorPos.y);
                    dist = sqrtf(ddx * ddx + ddy * ddy);
                }
                g_lastOriginCursorPos = pt;
                g_lastOriginCursorValid = true;

                if (!g_originInitialized) {
                    // Snap to the current offset on first use (no glide from 0,0).
                    g_smoothedOffsetX = (float)target.x;
                    g_smoothedOffsetY = (float)target.y;
                    g_originTarget = target;
                    g_originInitialized = true;
                } else if (!g_originTransitioning &&
                           (target.x != g_originTarget.x || target.y != g_originTarget.y)) {
                    // Target changed — start an ease-in-out transition from the
                    // current smoothed value.
                    g_originFromX = g_smoothedOffsetX;
                    g_originFromY = g_smoothedOffsetY;
                    g_originTarget = target;
                    g_originStartTime = now;
                    g_originProgressDist = 0.0f;
                    g_originTransitioning = true;
                }

                // Advance progress: time-driven (time_based) or distance-driven
                // (size_based), over a third of the configured tail value.
                if (g_originTransitioning) {
                    float p;
                    if (g_sizeBased) {
                        g_originProgressDist += dist;
                        float len = (float)(g_tailSize / 3);
                        if (len < 1.0f) len = 1.0f;
                        p = g_originProgressDist / len;
                    } else {
                        float len = (float)(g_tailDuration / 3);
                        if (len < 1.0f) len = 1.0f;
                        p = (float)(now - g_originStartTime) / len;
                    }
                    if (p > 1.0f) p = 1.0f;
                    float e = Ease(p, INTERP_SMOOTHSTEP);
                    g_smoothedOffsetX = g_originFromX + (g_originTarget.x - g_originFromX) * e;
                    g_smoothedOffsetY = g_originFromY + (g_originTarget.y - g_originFromY) * e;
                    if (p >= 1.0f) g_originTransitioning = false;
                } else {
                    g_smoothedOffsetX = (float)target.x;
                    g_smoothedOffsetY = (float)target.y;
                }

                originOffset.x = RoundToLong(g_smoothedOffsetX);
                originOffset.y = RoundToLong(g_smoothedOffsetY);
            } else {
                std::lock_guard<std::mutex> offsetLock(g_offsetMutex);
                if (g_history.empty()) {
                    g_frozenCursorOffset = g_cursorVisualOffset;
                }
                originOffset = g_frozenCursorOffset;
            }

            POINT newPt = { pt.x + originOffset.x - vX,
                            pt.y + originOffset.y - vY };

            // === DUPLICATE-SKIP — only blocks push, not eviction ===
            // Eviction has already run above, so it is safe to continue here.
            if (!g_history.empty() &&
                g_history.front().pos.x == newPt.x &&
                g_history.front().pos.y == newPt.y) {
                // Cursor hasn't moved since last sample — skip push.
                continue;
            }

            // === PUSH ===
            Sample s;
            s.pos = newPt;
            s.t = now;
            g_history.push_front(s);
            g_lastMovementTime = now;
            g_isFading = false;
        }
    }
    return 0;
}

// Forward declaration: MMTimerCallback is defined later (before
// OverlayThreadProc). Forward-declare so the compiler knows the signature.
void CALLBACK MMTimerCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

VOID CALLBACK SmearTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    POINT pt;
    GetCursorPos(&pt);

    static DWORD lastFullscreenCheck = 0;
    static bool needsClear = false;

    // Dirty rect tracking: the previous frame's bounding box (in virtual-screen
    // coords) so we can erase the old trail when it moves. {0,0,0,0} = no
    // previous trail (first frame or after full clear).
    static RECT s_prevDirtyRect = { 0, 0, 0, 0 };
    static bool s_hasPrevDirty = false;

    // Periodically refresh the game-running detection and publish the result
    // to the polling thread via the atomic flag.
    if (dwTime - lastFullscreenCheck > 500) {
        bool gameNow = IsGameRunning();
        g_isGameRunning.store(gameNow);
        lastFullscreenCheck = dwTime;
    }

    // Read local copies of the atomic flags for consistent use within this frame.
    bool isGameCached = g_isGameRunning.load();

    int vX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vH = GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1;

    if (isGameCached) {
        {
            std::lock_guard<std::mutex> lock(g_historyMutex);
            bool wasEmpty = g_history.empty();
            g_history.clear();
            if (wasEmpty && !needsClear) {
                return;
            }
        }
    } else {
        // No game running — update cursor appearance caches. The poll thread
        // owns trail-origin selection (frozen when g_history is empty, unless
        // g_trailOriginMode is Immediate/Smooth) and sample accumulation.
        // Time-based eviction retracts the trail naturally when the cursor
        // stops. The render thread only draws what's in g_history.
        UpdateCursorCenterOffset();
    }

    // Snapshot the current history size to decide whether to draw.
    bool historyEmpty;
    {
        std::lock_guard<std::mutex> lock(g_historyMutex);
        historyEmpty = g_history.empty();
    }

    if (!historyEmpty || needsClear || (g_debugShowOutline && g_cursorBmWidth > 0 && g_cursorBmHeight > 0)) {
        HDC hdcScreen = GetDC(NULL);

        // Only allocate the massive bitmap once, or if the screen size physically changes
        if (!g_hBitmap || g_cachedVW != vW || g_cachedVH != vH) {
            if (g_hBitmap) DeleteObject(g_hBitmap);
            if (g_hdcMem) DeleteDC(g_hdcMem);

            g_hdcMem = CreateCompatibleDC(hdcScreen);
            g_hBitmap = CreateCompatibleBitmap(hdcScreen, vW, vH);
            SelectObject(g_hdcMem, g_hBitmap);

            g_cachedVW = vW;
            g_cachedVH = vH;

            // If the bitmap changed, the render target needs to be rebuilt to match it
            if (g_pDCRenderTarget) {
                if (g_pSimpleLineBrush) { g_pSimpleLineBrush->Release(); g_pSimpleLineBrush = nullptr; }
                if (g_pDebugBrush) { g_pDebugBrush->Release(); g_pDebugBrush = nullptr; }
                if (g_pDebugBrushRed) { g_pDebugBrushRed->Release(); g_pDebugBrushRed = nullptr; }
                if (g_pDebugBrushBlue) { g_pDebugBrushBlue->Release(); g_pDebugBrushBlue = nullptr; }
                g_pDCRenderTarget->Release();
                g_pDCRenderTarget = nullptr;
            }
        }

        // Initialize Direct2D Render Target if we don't have one
        if (!g_pDCRenderTarget && g_pD2DFactory) {
            D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT
            );
            
            g_pD2DFactory->CreateDCRenderTarget(&props, &g_pDCRenderTarget);
            if (g_pDCRenderTarget) {
                g_pDCRenderTarget->CreateSolidColorBrush(
                    D2D1::ColorF(D2D1::ColorF::White), &g_pDebugBrush);
                g_pDCRenderTarget->CreateSolidColorBrush(
                    D2D1::ColorF(D2D1::ColorF::Red), &g_pDebugBrushRed);
                g_pDCRenderTarget->CreateSolidColorBrush(
                    D2D1::ColorF(D2D1::ColorF::Blue), &g_pDebugBrushBlue);
            }
        }

        // Track the current frame's trail bounding box (in backbuffer coords).
        // Declared here so it's visible after the render target block.
        RECT curBBox = { 0, 0, 0, 0 };
        bool hasCurBBox = false;

        if (g_pDCRenderTarget) {
            RECT rc = { 0, 0, vW, vH };
            g_pDCRenderTarget->BindDC(g_hdcMem, &rc);
            
            g_pDCRenderTarget->BeginDraw();
            g_pDCRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
            g_pDCRenderTarget->SetAntialiasMode(g_antialiasing
                ? D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
                : D2D1_ANTIALIAS_MODE_ALIASED);

            // Step 1 — Snapshot g_history and build trail points
            // Adaptive min distance: smaller kMinDist for longer trails so
            // decimation keeps enough waypoints. Scales inversely with kMaxPoints,
            // clamped to [2, 6].
            size_t kMaxPoints = g_sizeBased
                ? (size_t)g_tailSize
                : (size_t)g_tailDuration;
            if (kMaxPoints < 2) kMaxPoints = 2;
            float kMinDist = 6.0f * (10.0f / (float)kMaxPoints);
            if (kMinDist < 2.0f) kMinDist = 2.0f;
            if (kMinDist > 6.0f) kMinDist = 6.0f;

            std::vector<D2D1_POINT_2F> smoothed;
            smoothed.reserve(kMaxPoints);

            // Build the trail from g_history. The front of the deque is the most recent
            // sample (captured by the poll thread at ~1ms intervals with the cursor
            // center offset already applied). Using g_history.front() as the head
            // guarantees monotonic ordering — the head is always the newest point, and
            // all subsequent points are older. This avoids the trail doubling back on
            // itself when the render thread's GetCursorPos is stale relative to the
            // poll thread's latest sample.
            {
                std::lock_guard<std::mutex> lock(g_historyMutex);
                if (g_history.empty()) {
                    // No history yet — fall back to the render thread's cursor position.
                    // Read the origin offset under g_offsetMutex (nested inside
                    // g_historyMutex — consistent lock order everywhere).
                    POINT originOffset;
                    {
                        std::lock_guard<std::mutex> offsetLock(g_offsetMutex);
                        originOffset = (g_trailOriginMode == ORIGIN_NONE)
                            ? g_frozenCursorOffset
                            : g_cursorVisualOffset;
                    }
                    POINT headPt = { pt.x + originOffset.x - vX,
                                     pt.y + originOffset.y - vY };
                    smoothed.push_back(D2D1::Point2F(
                        (float)headPt.x + g_tailOffsetX,
                        (float)headPt.y + g_tailOffsetY));
                } else {
                    // Point 0: newest poll sample (front of deque)
                    smoothed.push_back(D2D1::Point2F(
                        (float)g_history.front().pos.x + g_tailOffsetX,
                        (float)g_history.front().pos.y + g_tailOffsetY));

                    // Spatial decimation: keep only points at least kMinDist
                    // pixels apart, producing evenly-spaced waypoints for
                    // consistent Chaikin smoothing.
                    D2D1_POINT_2F prev = smoothed[0];
                    for (const auto& s : g_history) {
                        if (smoothed.size() >= kMaxPoints) break;
                        float sx = (float)s.pos.x + g_tailOffsetX;
                        float sy = (float)s.pos.y + g_tailOffsetY;
                        float dx = sx - prev.x;
                        float dy = sy - prev.y;
                        if (dx * dx + dy * dy >= kMinDist * kMinDist)
                        {
                            smoothed.push_back(D2D1::Point2F(sx, sy));
                            prev = smoothed.back();
                        }
                    }

                }
            }

            bool isDrawing = (smoothed.size() >= 2);
            if (isDrawing) {
                // Step 2 — Chaikin subdivision: smooths corners by inserting
                // intermediate points, roughly doubling count per iteration.
                for (int iter = 0; iter < 2; ++iter) {
                        if (smoothed.size() < 3) break;
                        std::vector<D2D1_POINT_2F> next_s;
                        next_s.reserve(smoothed.size() * 2);
                        next_s.push_back(smoothed.front());
                        for (size_t i = 0; i < smoothed.size() - 1; ++i) {
                            D2D1_POINT_2F p0 = smoothed[i];
                            D2D1_POINT_2F p1 = smoothed[i+1];
                            next_s.push_back(D2D1::Point2F(2.0f/3.0f * p0.x + 1.0f/3.0f * p1.x, 2.0f/3.0f * p0.y + 1.0f/3.0f * p1.y));
                            next_s.push_back(D2D1::Point2F(1.0f/3.0f * p0.x + 2.0f/3.0f * p1.x, 1.0f/3.0f * p0.y + 2.0f/3.0f * p1.y));
                        }
                        next_s.push_back(smoothed.back());
                        smoothed.swap(next_s);
                    }

                // Step 3 — Compute trail bounding box
                float minX = smoothed[0].x, maxX = smoothed[0].x;
                float minY = smoothed[0].y, maxY = smoothed[0].y;
                for (const auto& p : smoothed) {
                    if (p.x < minX) minX = p.x;
                    if (p.x > maxX) maxX = p.x;
                    if (p.y < minY) minY = p.y;
                    if (p.y > maxY) maxY = p.y;
                }
                // Expand for stroke width / anti-aliasing
                int margin = 32;
                if (!g_simpleLineWidths.empty()) {
                    float maxW = g_simpleLineWidths[0];
                    for (float w : g_simpleLineWidths) {
                        if (w > maxW) maxW = w;
                    }
                    margin = (int)maxW + 16;
                }
                curBBox.left   = (LONG)minX - margin;
                curBBox.top    = (LONG)minY - margin;
                curBBox.right  = (LONG)maxX + margin;
                curBBox.bottom = (LONG)maxY + margin;
                hasCurBBox = true;

                // Step 4 — Dispatch to active style renderer
                if (g_activeStyle == L"simple_line") {
                    RenderSimpleLineStyle(smoothed);
                }
                // Future styles: add else-if branches here, e.g.
                // else if (g_activeStyle == L"glow") { RenderGlowStyle(smoothed); }

                needsClear = true; 
            } else {
                needsClear = false; 
            }

            // DEBUG: draw outline boxes around the detected cursor bitmap and
            // its visible (alpha-trimmed) pixels, when enabled in settings.
            if (g_debugShowOutline && g_cursorBmWidth > 0 && g_cursorBmHeight > 0) {
                POINT centerOffset;
                {
                    std::lock_guard<std::mutex> offsetLock(g_offsetMutex);
                    centerOffset = g_cursorCenterOffset;
                }
                float boxW = g_cursorBmWidth * g_cursorDpiScaleX;
                float boxH = g_cursorBmHeight * g_cursorDpiScaleY;
                float boxCx = (float)(pt.x + centerOffset.x - vX);
                float boxCy = (float)(pt.y + centerOffset.y - vY);
                D2D1_RECT_F debugBox = D2D1::RectF(boxCx - boxW / 2.0f,
                                                   boxCy - boxH / 2.0f,
                                                   boxCx + boxW / 2.0f,
                                                   boxCy + boxH / 2.0f);
                if (g_pDebugBrush) {
                    g_pDCRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
                    g_pDCRenderTarget->DrawRectangle(debugBox, g_pDebugBrush, 1.0f);
                }
                // DEBUG: red box around the alpha-trimmed (visible) cursor pixels.
                if (g_cursorVisibleValid && g_pDebugBrushRed) {
                    float baseX = boxCx - boxW / 2.0f;
                    float baseY = boxCy - boxH / 2.0f;
                    D2D1_RECT_F visBox = D2D1::RectF(
                        baseX + g_cursorVisLeft * g_cursorDpiScaleX,
                        baseY + g_cursorVisTop * g_cursorDpiScaleY,
                        baseX + g_cursorVisRight * g_cursorDpiScaleX,
                        baseY + g_cursorVisBottom * g_cursorDpiScaleY);
                    g_pDCRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
                    g_pDCRenderTarget->DrawRectangle(visBox, g_pDebugBrushRed, 1.0f);
                }
                // Include the box in the dirty rect so it is always blitted.
                LONG dbgL = (LONG)debugBox.left - 1;
                LONG dbgT = (LONG)debugBox.top - 1;
                LONG dbgR = (LONG)debugBox.right + 1;
                LONG dbgB = (LONG)debugBox.bottom + 1;
                if (hasCurBBox) {
                    if (dbgL < curBBox.left) curBBox.left = dbgL;
                    if (dbgT < curBBox.top) curBBox.top = dbgT;
                    if (dbgR > curBBox.right) curBBox.right = dbgR;
                    if (dbgB > curBBox.bottom) curBBox.bottom = dbgB;
                } else {
                    curBBox = { dbgL, dbgT, dbgR, dbgB };
                    hasCurBBox = true;
                }
            }

            // DEBUG: blue "+" marking the exact trail start (head point).
            if (g_debugShowOutline && !smoothed.empty() && g_pDebugBrushBlue) {
                float hx = smoothed[0].x;
                float hy = smoothed[0].y;
                const float half = 5.0f;
                g_pDCRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
                g_pDCRenderTarget->DrawLine(D2D1::Point2F(hx - half, hy),
                                            D2D1::Point2F(hx + half, hy), g_pDebugBrushBlue, 1.0f);
                g_pDCRenderTarget->DrawLine(D2D1::Point2F(hx, hy - half),
                                            D2D1::Point2F(hx, hy + half), g_pDebugBrushBlue, 1.0f);
                // Include the marker in the dirty rect so it is always blitted.
                LONG pL = (LONG)hx - (LONG)half - 1;
                LONG pT = (LONG)hy - (LONG)half - 1;
                LONG pR = (LONG)hx + (LONG)half + 1;
                LONG pB = (LONG)hy + (LONG)half + 1;
                if (hasCurBBox) {
                    if (pL < curBBox.left) curBBox.left = pL;
                    if (pT < curBBox.top) curBBox.top = pT;
                    if (pR > curBBox.right) curBBox.right = pR;
                    if (pB > curBBox.bottom) curBBox.bottom = pB;
                } else {
                    curBBox = { pL, pT, pR, pB };
                    hasCurBBox = true;
                }
            }

            HRESULT hr = g_pDCRenderTarget->EndDraw();
            if (hr == D2DERR_RECREATE_TARGET) {
                if (g_pSimpleLineBrush) { g_pSimpleLineBrush->Release(); g_pSimpleLineBrush = nullptr; }
                if (g_pDebugBrush) { g_pDebugBrush->Release(); g_pDebugBrush = nullptr; }
                if (g_pDebugBrushRed) { g_pDebugBrushRed->Release(); g_pDebugBrushRed = nullptr; }
                if (g_pDebugBrushBlue) { g_pDebugBrushBlue->Release(); g_pDebugBrushBlue = nullptr; }
                g_pDCRenderTarget->Release();
                g_pDCRenderTarget = nullptr;
            }
        }

        // Step 5 — Blit backbuffer to overlay window (dirty rect or full screen)
        // Skip UpdateLayeredWindow entirely if there's nothing to show
        // (no current trail, no previous trail to erase, no clear needed).
        // This prevents full-screen compositor updates every frame when
        // the cursor is stationary — which caused explorer crashes.
        if (!hasCurBBox && !s_hasPrevDirty && !needsClear) {
            ReleaseDC(NULL, hdcScreen);
            return;
        }

        // Compute the dirty rect: union of current + previous bounding boxes.
        // Both are in backbuffer coords (origin at 0,0 = virtual screen origin).
        RECT dirtyRect;
        bool useDirtyRect = false;

        if (hasCurBBox && s_hasPrevDirty) {
            // Union of current and previous
            dirtyRect.left   = (curBBox.left   < s_prevDirtyRect.left)   ? curBBox.left   : s_prevDirtyRect.left;
            dirtyRect.top    = (curBBox.top    < s_prevDirtyRect.top)    ? curBBox.top    : s_prevDirtyRect.top;
            dirtyRect.right  = (curBBox.right  > s_prevDirtyRect.right)  ? curBBox.right  : s_prevDirtyRect.right;
            dirtyRect.bottom = (curBBox.bottom > s_prevDirtyRect.bottom) ? curBBox.bottom : s_prevDirtyRect.bottom;
            useDirtyRect = true;
        } else if (hasCurBBox) {
            dirtyRect = curBBox;
            useDirtyRect = true;
        } else if (s_hasPrevDirty) {
            // No current trail, but previous frame had one — erase it
            dirtyRect = s_prevDirtyRect;
            useDirtyRect = true;
        }

        // Clamp dirty rect to backbuffer bounds
        if (useDirtyRect) {
            if (dirtyRect.left < 0) dirtyRect.left = 0;
            if (dirtyRect.top < 0) dirtyRect.top = 0;
            if (dirtyRect.right > vW) dirtyRect.right = vW;
            if (dirtyRect.bottom > vH) dirtyRect.bottom = vH;

            // Check the 768x768 cap — if exceeded, fall back to full screen
            int dirtyW = dirtyRect.right - dirtyRect.left;
            int dirtyH = dirtyRect.bottom - dirtyRect.top;
            if (dirtyW > 768 || dirtyH > 768 || dirtyW <= 0 || dirtyH <= 0) {
                useDirtyRect = false;  // fall back to full screen
            }
        }

        // Update previous-frame tracking for next tick
        if (hasCurBBox) {
            s_prevDirtyRect = curBBox;
            s_hasPrevDirty = true;
        } else if (!needsClear) {
            // Trail fully gone and cleared — reset previous tracking
            s_prevDirtyRect = { 0, 0, 0, 0 };
            s_hasPrevDirty = false;
        }

        BLENDFUNCTION blend = { 0 };
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255; 
        blend.AlphaFormat = AC_SRC_ALPHA;

        if (useDirtyRect) {
            // Dirty rect: position the window at the dirty rect's screen
            // position, size it to the dirty rect, and source from the
            // corresponding offset in the backbuffer.
            POINT ptPos = { vX + dirtyRect.left, vY + dirtyRect.top };
            SIZE sizeWnd = { dirtyRect.right - dirtyRect.left,
                             dirtyRect.bottom - dirtyRect.top };
            POINT ptSrc = { dirtyRect.left, dirtyRect.top };
            UpdateLayeredWindow(hwnd, hdcScreen, &ptPos, &sizeWnd,
                                g_hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
        } else {
            // Full screen fallback
            POINT ptPos = { vX, vY };
            SIZE sizeWnd = { vW, vH };
            POINT ptSrc = { 0, 0 };
            UpdateLayeredWindow(hwnd, hdcScreen, &ptPos, &sizeWnd,
                                g_hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
        }

        ReleaseDC(NULL, hdcScreen);
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
        g_renderScheduled.store(false);
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
    // Coalesce: only post if no render is already pending. PostMessage does not
    // coalesce like SetTimer, so an unthrottled post every 8ms builds an
    // unbounded WM_TIMER backlog whenever a frame runs long.
    if (g_overlayHwnd && !g_renderScheduled.exchange(true)) {
        if (!PostMessage(g_overlayHwnd, WM_TIMER, 1, 0)) {
            // Window is gone or queue failed; clear so future renders aren't
            // permanently blocked.
            g_renderScheduled.store(false);
        }
    }
}

DWORD WINAPI OverlayThreadProc(LPVOID lpParam) {
    // Direct2D demands COM to be initialized on this thread before it will talk to us
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // Tell Windows we aren't a blurry legacy piece of shit so mixed-DPI monitors don't fuck up the math
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory);

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

    g_overlayHwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        CLASS_NAME,
        L"SmearOverlay",
        WS_POPUP,
        screenX, screenY, screenW, screenH,
        NULL, NULL, hInstance, NULL
    );

    if (!g_overlayHwnd) return 0;

    ShowWindow(g_overlayHwnd, SW_SHOWNA);

    // Start the high-frequency cursor polling thread.
    // The stop event is a manual-reset event, initially non-signalled.
    g_pollStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_pollStopEvent) {
        g_pollThread = CreateThread(NULL, 0, PollThreadProc, NULL, 0, NULL);
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
    g_mmTimerId = timeSetEvent(kRenderIntervalMs, 1, MMTimerCallback, 0, TIME_PERIODIC);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Stop the polling thread gracefully before tearing down D2D resources.
    if (g_pollStopEvent) {
        SetEvent(g_pollStopEvent);
    }
    if (g_pollThread) {
        WaitForSingleObject(g_pollThread, 200);
        CloseHandle(g_pollThread);
        g_pollThread = NULL;
    }
    if (g_pollStopEvent) {
        CloseHandle(g_pollStopEvent);
        g_pollStopEvent = NULL;
    }

    // Clean up our massive GPU footprint before checking out
    if (g_pSimpleLineBrush) { g_pSimpleLineBrush->Release(); g_pSimpleLineBrush = nullptr; }
    if (g_pDebugBrush) { g_pDebugBrush->Release(); g_pDebugBrush = nullptr; }
    if (g_pDebugBrushRed) { g_pDebugBrushRed->Release(); g_pDebugBrushRed = nullptr; }
    if (g_pDebugBrushBlue) { g_pDebugBrushBlue->Release(); g_pDebugBrushBlue = nullptr; }
    if (g_pStrokeStyle) { g_pStrokeStyle->Release(); g_pStrokeStyle = nullptr; }
    if (g_pDCRenderTarget) { g_pDCRenderTarget->Release(); g_pDCRenderTarget = nullptr; }
    if (g_pD2DFactory) { g_pD2DFactory->Release(); g_pD2DFactory = nullptr; }

    if (g_hBitmap) DeleteObject(g_hBitmap);
    if (g_hdcMem) DeleteDC(g_hdcMem);

    // Kill the multimedia timer if still running (may have been killed
    // already by WhTool_ModUninit) and restore default timer resolution.
    if (g_mmTimerId) {
        timeKillEvent(g_mmTimerId);
        g_mmTimerId = 0;
    }
    timeEndPeriod(1);

    DestroyWindow(g_overlayHwnd);
    UnregisterClass(CLASS_NAME, hInstance);

    CoUninitialize();
    return 0;
}

BOOL WhTool_ModInit() {
    LoadSettings();
    g_threadHandle = CreateThread(NULL, 0, OverlayThreadProc, NULL, 0, &g_overlayThreadId);
    return TRUE;
}

void WhTool_ModUninit() {
    // Signal the polling thread to stop. The overlay thread's cleanup block
    // will also signal it and wait, but signalling here first ensures the poll
    // thread begins shutting down before the overlay window's WM_QUIT is posted.
    if (g_pollStopEvent) {
        SetEvent(g_pollStopEvent);
    }

    // Kill the multimedia timer BEFORE posting WM_QUIT. The timer posts
    // WM_TIMER messages at 125Hz; if left running, they flood the message
    // queue and starve WM_QUIT, causing the unload to hang forever.
    if (g_mmTimerId) {
        timeKillEvent(g_mmTimerId);
        g_mmTimerId = 0;
        Sleep(20);  // Let any in-flight MMTimerCallback fire and complete
    }

    if (g_overlayThreadId) {
        PostThreadMessage(g_overlayThreadId, WM_QUIT, 0, 0);
    }
    if (g_threadHandle) {
        DWORD waitResult = WaitForSingleObject(g_threadHandle, 5000);
        if (waitResult == WAIT_TIMEOUT) {
            // Safety net: if the overlay thread didn't exit cleanly in 5s,
            // force-terminate to avoid hanging Windhawk's unload.
            TerminateThread(g_threadHandle, 0);
        }
        CloseHandle(g_threadHandle);
        g_threadHandle = NULL;
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