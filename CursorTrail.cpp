// ==WindhawkMod==
// @id              cursor-trail
// @name            Cursor trail
// @description     Cursor trail overlay with configurable styles (simple line, cursor ghost)
// @version         0.11
// @author          Ulrizza
// @license         MIT
// @include         windhawk.exe
// @compilerOptions -ld2d1 -lole32 -lgdi32 -lshell32 -lwindowscodecs -lwinmm
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*...*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- style: simple_line
  $name: Style
  $description: Trail rendering style (simple_line or cursor_ghost)
  $options:
  - simple_line: Simple line
  - cursor_ghost: Cursor ghost
- simpleLineOptions:
  - trail_mode: "time_based"
    $name: Trail mode
    $description: Time based fades the trail over time; Size based keeps a fixed number of trail points even when stationary
    $options:
    - time_based: Time based
    - size_based: Size based
  - antialiasing: "true"
    $name: Antialiasing
    $description: Smooth the trail edges (disable for hard, pixelated edges)
    $options:
    - "true": "True"
    - "false": "False"
  - timeBased:
    - tail_duration: 500
      $name: Tail duration
      $description: Milliseconds each trail segment stays visible (minimum 20)
    $name: Time based
  - sizeBased:
    - tail_size: 2000
      $name: Tail length
      $description: Maximum trail length in pixels (minimum 20)
    - timeout: 0
      $name: Timeout
      $description: Milliseconds of inactivity before the trail starts fading (using the Time based tail duration). 0 = trail always visible.
    $name: Size based
  - width:
    - values: "2,1"
      $name: Values
      $description: "Comma-separated stroke widths from head to tail (e.g. \"2,1\" for a tapered trail, or \"10,1,10,1\" for a pulsing trail). Each value gets an equal share; repeat to widen (e.g. \"2,2,2,2,1\" = 80%% at 2, 20%% at 1). Minimum 1."
    $name: Width
  - color:
    - values: "000000"
      $name: Values
      $description: "Line color for the Simple line style. Single hex (RRGGBB without #, e.g. 000000 for black) or comma-separated list for a gradient from head to tail (e.g. 000000,FF0000,FFFFFF for black->red->white). Each color gets an equal share; repeat to widen. Invalid entries are skipped."
    - blend_width: 0
      $name: Blend width
      $description: Percentage of each transition spent blending (0 = pure bands, 100 = full gradient). 50 with red,blue gives 25% hard red, 50% blend, 25% hard blue.
    - interpolation: "smoothstep"
      $name: Interpolation
      $description: Blending curve between colors
      $options:
      - linear: Linear
      - smoothstep: Smoothstep
      - ease_in: Ease in
      - ease_out: Ease out
    $name: Color
  - opacity:
    - values: "100,0"
      $name: Values
      $description: Comma-separated opacity percentages (0-100) from head to tail (e.g. "100,0" for full fade, or "100,0,100" for a pulse). Each value gets an equal share; repeat to widen. Leave one value for uniform opacity.
    $name: Opacity
  $name: Simple line options
- tail_offset:
  - x: 0
    $name: X
    $description: Fine-tune the trail origin horizontally (auto-centered by default, 0 = no adjustment)
  - y: 0
    $name: Y
    $description: Fine-tune the trail origin vertically (auto-centered by default, 0 = no adjustment)
  $name: Trail offset
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <d2d1.h>
#include <math.h>
#include <shellapi.h>
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
std::wstring g_trailMode = L"time_based";
int g_tailSize = 2000;
bool g_antialiasing = true;

DWORD g_sizeTimeout = 0;
DWORD g_lastMovementTime = 0;
bool g_isFading = false;

// Active rendering style (copied from Wh_GetStringSetting, freed immediately)
std::wstring g_activeStyle = L"simple_line";

// Simple line style settings
std::vector<float> g_simpleLineWidths;       // parsed width values, one per stop
std::wstring g_simpleLineColorHex = L"000000";
std::vector<std::wstring> g_simpleLineColors;  // parsed from g_simpleLineColorHex, split on commas

struct Rgb { float r, g, b; };
std::vector<Rgb> g_simpleLineColorsRGB;         // pre-parsed for hot-path use

int g_colorBlendWidth = 0;
std::wstring g_colorInterpolation = L"smoothstep";

std::vector<float> g_simpleLineOpacityValues; // parsed opacity values (0-100)

ID2D1SolidColorBrush* g_pSimpleLineBrush = nullptr;
ID2D1StrokeStyle* g_pStrokeStyle = nullptr;

// Cursor visual-center cache (avoids re-querying GetIconInfo every frame
// when the cursor handle hasn't changed)
HCURSOR g_cachedCursor = NULL;
POINT g_cursorCenterOffset = { 0, 0 };  // added to hotspot to reach bitmap center

// Frozen cursor center offset, snapshotted by the poll thread when a new
// trail starts (g_history is empty and a new sample is about to be pushed).
// All samples in a trail share the same coordinate space, even if the cursor
// shape changes mid-trail (e.g. arrow → I-beam). g_cursorCenterOffset is
// updated by the render thread; g_frozenCursorOffset is read and written by
// the poll thread.
POINT g_frozenCursorOffset = { 0, 0 };

std::mutex g_offsetMutex;  // protects g_cursorCenterOffset and g_frozenCursorOffset
                            // lock order: g_historyMutex (if needed) THEN g_offsetMutex


// Cursor ghost style: cached D2D bitmap of the current cursor, rebuilt only
// when the HCURSOR changes. g_cursorBitmap is owned by the render target and
// must be released before the render target is released/recreated.
ID2D1Bitmap* g_cursorBitmap = nullptr;
int g_cursorBmWidth = 0;
int g_cursorBmHeight = 0;
float g_cursorDpiScaleX = 1.0f;
float g_cursorDpiScaleY = 1.0f;

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

void LoadSettings() {
    g_tailOffsetX = Wh_GetIntSetting(L"tail_offset.x");
    g_tailOffsetY = Wh_GetIntSetting(L"tail_offset.y");

    // Trail mode
    PCWSTR modeSetting = Wh_GetStringSetting(L"simpleLineOptions.trail_mode");
    if (modeSetting && *modeSetting) {
        g_trailMode = modeSetting;
    } else {
        g_trailMode = L"time_based";
    }
    if (modeSetting) Wh_FreeStringSetting(modeSetting);

    PCWSTR aaSetting = Wh_GetStringSetting(L"simpleLineOptions.antialiasing");
    g_antialiasing = !aaSetting || wcscmp(aaSetting, L"false") != 0;
    if (aaSetting) Wh_FreeStringSetting(aaSetting);

    g_tailDuration = Wh_GetIntSetting(L"simpleLineOptions.timeBased.tail_duration");
    g_tailSize = Wh_GetIntSetting(L"simpleLineOptions.sizeBased.tail_size");
    g_sizeTimeout = Wh_GetIntSetting(L"simpleLineOptions.sizeBased.timeout");

    // Read the style setting (string — must be freed with Wh_FreeStringSetting)
    PCWSTR styleSetting = Wh_GetStringSetting(L"style");
    if (styleSetting && *styleSetting) {
        g_activeStyle = styleSetting;
    } else {
        g_activeStyle = L"simple_line";  // fallback per RG-3
    }
    if (styleSetting) Wh_FreeStringSetting(styleSetting);

    // RG-3: unknown style value → fallback to simple_line
    if (g_activeStyle != L"simple_line" && g_activeStyle != L"cursor_ghost") {
        g_activeStyle = L"simple_line";
    }

    if (g_tailDuration < 20) g_tailDuration = 20;
    if (g_tailSize < 20) g_tailSize = 20;
    if (g_sizeTimeout < 0) g_sizeTimeout = 0;

    // Parse width values
    g_simpleLineWidths.clear();
    {
        PCWSTR widthSetting = Wh_GetStringSetting(L"simpleLineOptions.width.values");
        std::vector<std::wstring> widthTokens;
        if (widthSetting && *widthSetting) {
            widthTokens = SplitAndTrim(std::wstring(widthSetting));
        }
        if (widthTokens.empty()) {
            widthTokens.push_back(L"1");
        }
        for (const auto& tok : widthTokens) {
            try {
                float w = std::stof(tok);
                if (w < 1.0f) w = 1.0f;
                g_simpleLineWidths.push_back(w);
            } catch (...) {
                g_simpleLineWidths.push_back(1.0f);
            }
        }
        if (widthSetting) Wh_FreeStringSetting(widthSetting);
    }

    // Parse opacity values
    g_simpleLineOpacityValues.clear();
    {
        PCWSTR opacSetting = Wh_GetStringSetting(L"simpleLineOptions.opacity.values");
        std::vector<std::wstring> opacTokens;
        if (opacSetting && *opacSetting) {
            opacTokens = SplitAndTrim(std::wstring(opacSetting));
        }
        if (opacTokens.empty()) {
            opacTokens.push_back(L"100");
        }
        for (const auto& tok : opacTokens) {
            try {
                float v = std::stof(tok);
                if (v < 0.0f) v = 0.0f;
                if (v > 100.0f) v = 100.0f;
                g_simpleLineOpacityValues.push_back(v);
            } catch (...) {
                g_simpleLineOpacityValues.push_back(100.0f);
            }
        }
        if (opacSetting) Wh_FreeStringSetting(opacSetting);
    }

    // Parse color values
    PCWSTR colorSetting = Wh_GetStringSetting(L"simpleLineOptions.color.values");
    if (colorSetting && *colorSetting) {
        g_simpleLineColorHex = colorSetting;
    } else {
        g_simpleLineColorHex = L"000000";
    }
    if (colorSetting) Wh_FreeStringSetting(colorSetting);

    g_simpleLineColors.clear();
    g_simpleLineColors = SplitAndTrim(g_simpleLineColorHex);
    if (g_simpleLineColors.empty()) {
        g_simpleLineColors.push_back(L"000000");
    }

    g_simpleLineColorsRGB.clear();
    for (const auto& hex : g_simpleLineColors) {
        Rgb rgb;
        if (!ParseHexColor(hex, rgb.r, rgb.g, rgb.b)) {
            rgb = { 0, 0, 0 };
        }
        g_simpleLineColorsRGB.push_back(rgb);
    }

    g_colorBlendWidth = Wh_GetIntSetting(L"simpleLineOptions.color.blend_width");
    if (g_colorBlendWidth < 0) g_colorBlendWidth = 0;
    if (g_colorBlendWidth > 100) g_colorBlendWidth = 100;

    PCWSTR interpSetting = Wh_GetStringSetting(L"simpleLineOptions.color.interpolation");
    if (interpSetting && *interpSetting) {
        g_colorInterpolation = interpSetting;
    } else {
        g_colorInterpolation = L"smoothstep";
    }
    if (interpSetting) Wh_FreeStringSetting(interpSetting);
}

static void FreeIconInfoBitmaps(ICONINFO& ii) {
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask)  DeleteObject(ii.hbmMask);
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

// Compute the offset from the cursor's hotspot to the visual center of its bitmap.
// Cached per-HCURSOR so we only do the work when the cursor shape actually changes.
void UpdateCursorCenterOffset() {
    std::lock_guard<std::mutex> lock(g_offsetMutex);

    CURSORINFO ci = { sizeof(CURSORINFO) };
    if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING) || !ci.hCursor) {
        g_cursorCenterOffset = { 0, 0 };
        g_cachedCursor = NULL;
        return;
    }

    if (ci.hCursor == g_cachedCursor) {
        return;  // same cursor as last frame, reuse cached offset
    }

    ICONINFO ii = { };
    if (!GetIconInfo(ci.hCursor, &ii)) {
        g_cursorCenterOffset = { 0, 0 };
        g_cachedCursor = NULL;
        return;
    }

    int bmWidth = 0, bmHeight = 0;
    HBITMAP hbmToUse = NULL;
    ResolveCursorBitmapDimensions(ii, bmWidth, bmHeight, hbmToUse);

    if (bmWidth > 0 && bmHeight > 0) {
        float sx = (float)GetSystemMetrics(SM_CXCURSOR) / (float)bmWidth;
        float sy = (float)GetSystemMetrics(SM_CYCURSOR) / (float)bmHeight;
        if (sx < 1.0f) sx = 1.0f;
        if (sy < 1.0f) sy = 1.0f;
        g_cursorCenterOffset.x = (int)(((bmWidth / 2.0f) - (int)ii.xHotspot) * sx + 0.5f);
        g_cursorCenterOffset.y = (int)(((bmHeight / 2.0f) - (int)ii.yHotspot) * sy + 0.5f);
    } else {
        g_cursorCenterOffset = { 0, 0 };
    }

    FreeIconInfoBitmaps(ii);

    g_cachedCursor = ci.hCursor;
}

// Rebuild the cached D2D bitmap of the current cursor. Called when the cursor
// shape changes (HCURSOR differs from g_cachedCursor). Uses WIC to convert the
// GDI HBITMAP from GetIconInfo into an ID2D1Bitmap compatible with the render
// target. Monochrome cursors (no hbmColor) are handled via the mask bitmap.
void UpdateCursorBitmap() {
    CURSORINFO ci = { sizeof(CURSORINFO) };
    if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING) || !ci.hCursor) {
        if (g_cursorBitmap) { g_cursorBitmap->Release(); g_cursorBitmap = nullptr; }
        g_cursorBmWidth = 0;
        g_cursorBmHeight = 0;
        return;
    }

    // Reuse the cache check from UpdateCursorCenterOffset: if the HCURSOR
    // hasn't changed and we already have a bitmap, nothing to do.
    if (ci.hCursor == g_cachedCursor && g_cursorBitmap) {
        return;
    }

    ICONINFO ii = { };
    if (!GetIconInfo(ci.hCursor, &ii)) {
        if (g_cursorBitmap) { g_cursorBitmap->Release(); g_cursorBitmap = nullptr; }
        g_cursorBmWidth = 0;
        g_cursorBmHeight = 0;
        return;
    }

    int bmWidth = 0, bmHeight = 0;
    HBITMAP hbmToUse = NULL;
    ResolveCursorBitmapDimensions(ii, bmWidth, bmHeight, hbmToUse);

    if (!hbmToUse || bmWidth <= 0 || bmHeight <= 0) {
        FreeIconInfoBitmaps(ii);
        if (g_cursorBitmap) { g_cursorBitmap->Release(); g_cursorBitmap = nullptr; }
        g_cursorBmWidth = 0;
        g_cursorBmHeight = 0;
        return;
    }

    // Convert the HBITMAP to an ID2D1Bitmap via WIC.
    // Steps: HBITMAP → IWICBitmapSource (via IWICImagingFactory::CreateBitmapFromHBITMAP)
    //      → ID2D1Bitmap (via render target CreateBitmapFromWicBitmap)
    if (g_cursorBitmap) { g_cursorBitmap->Release(); g_cursorBitmap = nullptr; }

    if (g_pDCRenderTarget) {
        // Create WIC factory
        IWICImagingFactory* pWicFactory = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&pWicFactory));
        if (SUCCEEDED(hr) && pWicFactory) {
            IWICBitmap* pWicBitmap = nullptr;
            hr = pWicFactory->CreateBitmapFromHBITMAP(hbmToUse, NULL,
                WICBitmapUsePremultipliedAlpha, &pWicBitmap);
            if (SUCCEEDED(hr) && pWicBitmap) {
                UINT w = 0, h = 0;
                pWicBitmap->GetSize(&w, &h);
                // Create the D2D bitmap from the WIC bitmap
                hr = g_pDCRenderTarget->CreateBitmapFromWicBitmap(pWicBitmap, nullptr, &g_cursorBitmap);
                if (SUCCEEDED(hr) && g_cursorBitmap) {
                    g_cursorBmWidth = (int)w;
                    g_cursorBmHeight = (int)h;
                    g_cursorDpiScaleX = (float)GetSystemMetrics(SM_CXCURSOR) / (float)w;
                    g_cursorDpiScaleY = (float)GetSystemMetrics(SM_CYCURSOR) / (float)h;
                    if (g_cursorDpiScaleX < 1.0f) g_cursorDpiScaleX = 1.0f;
                    if (g_cursorDpiScaleY < 1.0f) g_cursorDpiScaleY = 1.0f;
                }
                pWicBitmap->Release();
            }
            pWicFactory->Release();
        }
    }

    FreeIconInfoBitmaps(ii);
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

// Blend model: each of the N-1 transitions gets a blend zone of
// width (blendWidth/100)/(N-1), centered on the boundary. Outside
// blend zones, pure colors. Inside, smoothstep. Overlapping zones
// merge into a multi-color gradient.
static void GetBlendedColor(const std::vector<Rgb>& colors,
                            int blendWidth,
                            float ratio,
                            float& r, float& g, float& b) {
    r = 0.0f; g = 0.0f; b = 0.0f;
    if (colors.empty()) return;
    if (colors.size() == 1) {
        r = colors[0].r; g = colors[0].g; b = colors[0].b;
        return;
    }

    int N = (int)colors.size();
    float totalBlend = (float)blendWidth / 100.0f;
    float perTrans = totalBlend;
    float half = perTrans / 2.0f;

    // Check pure bands first
    for (int i = 0; i < N; ++i) {
        float bandStart = (i == 0) ? 0.0f : (float)i / (float)N + half;
        float bandEnd   = (i == N - 1) ? 1.0f : (float)(i + 1) / (float)N - half;
        if (bandEnd > bandStart && ratio >= bandStart && ratio <= bandEnd) {
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

    if (g_colorInterpolation == L"ease_in")     frac = frac * frac;
    else if (g_colorInterpolation == L"ease_out")    frac = 1.0f - (1.0f - frac) * (1.0f - frac);
    else if (g_colorInterpolation == L"smoothstep")  frac = frac * frac * (3.0f - 2.0f * frac);

    // Build positions for the colors involved in this blended region
    int numColors = lastZone - firstZone + 2;
    std::vector<float> pos;
    for (int i = 0; i < numColors; ++i)
        pos.push_back((float)i / (float)(numColors - 1));
    if (pos.back() < 1.0f) pos.push_back(1.0f);

    size_t idx = 0;
    for (size_t i = 0; i + 1 < pos.size(); ++i) {
        if (frac >= pos[i] && frac < pos[i + 1]) { idx = i; break; }
    }
    if (frac >= 1.0f) idx = pos.size() - 2;

    float f = (frac - pos[idx]) / (pos[idx + 1] - pos[idx]);

    const auto& c0 = colors[firstZone + (int)idx];
    const auto& c1 = colors[firstZone + (int)idx + 1];
    r = c0.r + (c1.r - c0.r) * f;
    g = c0.g + (c1.g - c0.g) * f;
    b = c0.b + (c1.b - c0.b) * f;
}

static float Clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

// Interpolates a single float value across equal shares, using the same
// smoothstep interpolation as GetBlendedColor.
static float InterpolateWidth(const std::vector<float>& values,
                              float ratio) {
    if (values.empty()) return 1.0f;
    if (values.size() == 1) return values[0];

    float n = (float)values.size();
    std::vector<float> pos;
    for (size_t i = 0; i < values.size(); ++i)
        pos.push_back((float)i / (n - 1.0f));
    if (pos.back() < 1.0f) pos.push_back(1.0f);

    if (ratio <= pos[0]) return values[0];
    if (ratio >= 1.0f) return values.back();

    size_t idx = 0;
    for (size_t i = 0; i + 1 < pos.size(); ++i) {
        if (ratio >= pos[i] && ratio < pos[i + 1]) {
            idx = i;
            break;
        }
    }

    float spanStart = pos[idx];
    float spanEnd   = pos[idx + 1];
    float span      = spanEnd - spanStart;
    if (span <= 0.0f) return values[idx];
    float frac = (ratio - spanStart) / span;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;

    frac = frac * frac * (3.0f - 2.0f * frac);

    size_t vi = (idx + 1 < values.size()) ? idx + 1 : idx;
    return values[idx] + (values[vi] - values[idx]) * frac;
}

static float InterpolateOpacity(const std::vector<float>& values,
                                 float ratio) {
    float raw = InterpolateWidth(values, ratio);
    float alpha = raw / 100.0f;
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
        float alpha = InterpolateOpacity(g_simpleLineOpacityValues, ratio);
        float strokeWidth = InterpolateWidth(g_simpleLineWidths, ratio);
        if (strokeWidth < 0.5f) strokeWidth = 0.5f;

        float gr, gg, gb;
        GetBlendedColor(g_simpleLineColorsRGB, g_colorBlendWidth, ratio, gr, gg, gb);

        g_pSimpleLineBrush->SetColor(D2D1::ColorF(gr * alpha, gg * alpha, gb * alpha, alpha));
        g_pDCRenderTarget->DrawLine(smoothed[i], smoothed[i + 1], g_pSimpleLineBrush,
                                    strokeWidth, g_pStrokeStyle);
    }
}

// Render the "Cursor ghost" style: stamped copies of the current cursor icon
// placed along the smoothed trail path, with opacity fading from the head
// (near the real cursor, most opaque) to the tail (transparent). Reuses the
// opacity settings.
void RenderCursorGhostStyle(const std::vector<D2D1_POINT_2F>& smoothed) {
    if (smoothed.empty()) return;
    if (!g_pDCRenderTarget) return;
    if (!g_cursorBitmap || g_cursorBmWidth <= 0 || g_cursorBmHeight <= 0) return;

    size_t n = smoothed.size();
    D2D1_RECT_F srcRect = D2D1::RectF(0.0f, 0.0f,
                                      (FLOAT)g_cursorBmWidth,
                                      (FLOAT)g_cursorBmHeight);

    for (size_t i = 0; i < n; ++i) {
        float ratio = (n > 1) ? (float)i / (float)(n - 1) : 0.0f;
        float alpha = InterpolateOpacity(g_simpleLineOpacityValues, ratio);

        float scaledW = g_cursorBmWidth * g_cursorDpiScaleX;
        float scaledH = g_cursorBmHeight * g_cursorDpiScaleY;

        // Center the cursor bitmap on the smoothed path point.
        // g_cursorCenterOffset shifts from hotspot to visual center.
        float cx = smoothed[i].x - scaledW / 2.0f;
        float cy = smoothed[i].y - scaledH / 2.0f;
        D2D1_RECT_F destRect = D2D1::RectF(cx, cy,
                                           cx + scaledW,
                                           cy + scaledH);

        g_pDCRenderTarget->DrawBitmap(g_cursorBitmap, &destRect, alpha,
                                      D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                      &srcRect);
    }
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
            if (g_trailMode == L"size_based") {
                if (g_sizeTimeout > 0 && g_lastMovementTime > 0 &&
                    now - g_lastMovementTime > g_sizeTimeout) {
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
                    while (!g_history.empty() && (now - g_history.back().t) > (DWORD)g_tailDuration)
                        g_history.pop_back();
                    const size_t kMaxSamples = (size_t)(g_tailDuration);
                    while (g_history.size() > kMaxSamples)
                        g_history.pop_back();
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
                while (!g_history.empty() && (now - g_history.back().t) > (DWORD)g_tailDuration)
                    g_history.pop_back();
                const size_t kMaxSamples = (size_t)(g_tailDuration);
                while (g_history.size() > kMaxSamples)
                    g_history.pop_back();
            }

            // === TRAIL START — snapshot frozen offset when history is empty ===
            // When a new trail starts, snapshot g_cursorCenterOffset so all
            // samples in this trail share the same coordinate space even if the
            // cursor shape changes mid-trail.
            if (g_history.empty()) {
                std::lock_guard<std::mutex> offsetLock(g_offsetMutex);
                g_frozenCursorOffset = g_cursorCenterOffset;
            }

            // Compute sample position with the frozen offset (inside the lock so
            // it sees the freshly updated g_frozenCursorOffset if trail just started).
            POINT newPt = { pt.x + g_frozenCursorOffset.x - vX,
                            pt.y + g_frozenCursorOffset.y - vY };

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
        // owns trail-start (snapshots g_frozenCursorOffset when g_history is
        // empty) and sample accumulation. Time-based eviction retracts the
        // trail naturally when the cursor stops. The render thread only draws
        // what's in g_history.
        UpdateCursorCenterOffset();
        UpdateCursorBitmap();
    }

    // Snapshot the current history size to decide whether to draw.
    bool historyEmpty;
    {
        std::lock_guard<std::mutex> lock(g_historyMutex);
        historyEmpty = g_history.empty();
    }

    if (!historyEmpty || needsClear) {
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
                if (g_cursorBitmap) { g_cursorBitmap->Release(); g_cursorBitmap = nullptr; }
                if (g_pSimpleLineBrush) { g_pSimpleLineBrush->Release(); g_pSimpleLineBrush = nullptr; }
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
            size_t kMaxPoints = (g_trailMode == L"size_based")
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
                    // Read g_frozenCursorOffset under g_offsetMutex (nested inside
                    // g_historyMutex — consistent lock order everywhere).
                    POINT frozenOffset;
                    {
                        std::lock_guard<std::mutex> offsetLock(g_offsetMutex);
                        frozenOffset = g_frozenCursorOffset;
                    }
                    POINT headPt = { pt.x + frozenOffset.x - vX,
                                     pt.y + frozenOffset.y - vY };
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
                // Expand for stroke width / cursor ghost bitmap / anti-aliasing
                int margin = 32;
                if (!g_simpleLineWidths.empty()) {
                    float maxW = g_simpleLineWidths[0];
                    for (float w : g_simpleLineWidths) {
                        if (w > maxW) maxW = w;
                    }
                    margin = (int)maxW + 16;
                }
                if (g_activeStyle == L"cursor_ghost" && g_cursorBmWidth > 0) {
                    margin = (g_cursorBmWidth > g_cursorBmHeight ? g_cursorBmWidth : g_cursorBmHeight) + 16;
                }
                curBBox.left   = (LONG)minX - margin;
                curBBox.top    = (LONG)minY - margin;
                curBBox.right  = (LONG)maxX + margin;
                curBBox.bottom = (LONG)maxY + margin;
                hasCurBBox = true;

                // Step 4 — Dispatch to active style renderer
                if (g_activeStyle == L"simple_line") {
                    RenderSimpleLineStyle(smoothed);
                } else if (g_activeStyle == L"cursor_ghost") {
                    RenderCursorGhostStyle(smoothed);
                }
                // Future styles: add else-if branches here, e.g.
                // else if (g_activeStyle == L"glow") { RenderGlowStyle(smoothed); }

                needsClear = true; 
            } else {
                needsClear = false; 
            }

            HRESULT hr = g_pDCRenderTarget->EndDraw();
            if (hr == D2DERR_RECREATE_TARGET) {
                if (g_cursorBitmap) { g_cursorBitmap->Release(); g_cursorBitmap = nullptr; }
                if (g_pSimpleLineBrush) { g_pSimpleLineBrush->Release(); g_pSimpleLineBrush = nullptr; }
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
    if (g_cursorBitmap) { g_cursorBitmap->Release(); g_cursorBitmap = nullptr; }
    if (g_pSimpleLineBrush) { g_pSimpleLineBrush->Release(); g_pSimpleLineBrush = nullptr; }
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