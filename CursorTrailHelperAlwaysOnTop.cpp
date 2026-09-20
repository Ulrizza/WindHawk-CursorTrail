// ==WindhawkMod==
// @id              cursor-trail-helper-always-on-top
// @name            Cursor trail helper - always on top
// @description     Places the Cursor trail overlay above the taskbar and Start menu by moving it to a higher Z-order band
// @version         1.0
// @author          Ulrizza
// @license         MIT
// @include         explorer.exe
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Cursor trail helper - always on top

Companion mod for **Cursor trail**. It exists only to solve a Windows
limitation: starting with Windows 8, windows live in fixed Z-order *bands*
(`ZBID`), and a window in a lower band can never be drawn above a window in a
higher band, no matter how `SetWindowPos`/`WS_EX_TOPMOST` is used.

The `Cursor trail` overlay is a normal desktop-band window, while the Windows 11
taskbar sits in `ZBID_IMMERSIVE_MOGO` (6) and the Start menu and notification
center in other immersive bands above the desktop band. This mod moves the
overlay into `ZBID_SYSTEM_TOOLS` (16), the same band Task Manager uses for
"Always on top", which is above all of them.

## How it works

This mod is injected into `explorer.exe` (which has the privileges required for
band manipulation) and:

1. Finds the overlay window by its class name (`SmearFrameOverlayClass`).
2. Calls the undocumented `SetWindowBand(hwnd, NULL, 16)`.
3. If that is denied, it hooks `NtUserEnableIAMAccess` to capture the IAM access
   key the shell uses, then retries with that key. Capturing the key requires
   pressing the **Win key** (or otherwise opening a shell surface) once after
   the mod loads.

The band is re-applied automatically if the overlay is recreated or the band is
reset.

## Notes

- Uses undocumented Windows APIs (`SetWindowBand`, `NtUserEnableIAMAccess`).
- Places the trail above the taskbar, Start menu, Task Manager and Alt-Tab.
  The overlay is click-through, so it never steals input.
- Requires the **Cursor trail** mod to be installed and enabled.
*/
// ==/WindhawkModReadme==

#include <windows.h>

// --- Z-order bands (ZBID) -------------------------------------------------
enum ZBID {
    ZBID_DESKTOP = 1,
    ZBID_IMMERSIVE_MOGO = 6,
    ZBID_SYSTEM_TOOLS = 16,
};

// --- Undocumented user32 APIs --------------------------------------------
typedef BOOL(WINAPI* SetWindowBand_t)(HWND hWnd, HWND hwndInsertAfter, DWORD dwBand);
typedef BOOL(WINAPI* GetWindowBand_t)(HWND hWnd, PDWORD pdwBand);
typedef BOOL(WINAPI* NtUserEnableIAMAccess_t)(ULONG64 key, BOOL enable);

static SetWindowBand_t pSetWindowBand;
static GetWindowBand_t pGetWindowBand;
static NtUserEnableIAMAccess_t pNtUserEnableIAMAccess;
static NtUserEnableIAMAccess_t pNtUserEnableIAMAccessOriginal;

// --- State ----------------------------------------------------------------
// Must match the overlay window class created by the main Cursor trail mod.
static const wchar_t* kOverlayClass = L"SmearFrameOverlayClass";
static const DWORD kTargetBand = ZBID_SYSTEM_TOOLS;

static volatile ULONG64 g_iamKey;
static HANDLE g_stopEvent;
static HANDLE g_workerThread;
static HWND g_lastBanded;
static bool g_hookInstalled;
static volatile bool g_unhookPending;

// Captures the IAM access key when the shell calls NtUserEnableIAMAccess.
// The hook is removed by the worker thread (not from here) once captured.
static BOOL WINAPI NtUserEnableIAMAccessHook(ULONG64 key, BOOL enable) {
    BOOL result = pNtUserEnableIAMAccessOriginal(key, enable);
    if (result && g_iamKey == 0 && key != 0) {
        g_iamKey = key;
        g_unhookPending = true;
        Wh_Log(L"IAM access key captured");
    }
    return result;
}

static bool ApplyBand(HWND hwnd) {
    if (g_iamKey) {
        pNtUserEnableIAMAccess(g_iamKey, TRUE);
        BOOL ok = pSetWindowBand(hwnd, nullptr, kTargetBand);
        DWORD err = ok ? ERROR_SUCCESS : GetLastError();
        pNtUserEnableIAMAccess(g_iamKey, FALSE);
        if (!ok) {
            Wh_Log(L"SetWindowBand (key) failed: %u", err);
        }
        return ok != FALSE;
    }

    // The shell may already have IAM access enabled; try without a key first.
    SetLastError(ERROR_SUCCESS);
    BOOL ok = pSetWindowBand(hwnd, nullptr, kTargetBand);
    if (!ok) {
        DWORD err = GetLastError();
        if (err != ERROR_ACCESS_DENIED) {
            Wh_Log(L"SetWindowBand (direct) failed: %u", err);
        }
        return false;
    }
    return true;
}

static DWORD WINAPI BandWorkerThread(LPVOID) {
    for (;;) {
        if (WaitForSingleObject(g_stopEvent, 1000) != WAIT_TIMEOUT) {
            break;
        }

        if (g_unhookPending && g_hookInstalled && pNtUserEnableIAMAccess) {
            Wh_RemoveFunctionHook((void*)pNtUserEnableIAMAccess);
            g_hookInstalled = false;
            g_unhookPending = false;
            Wh_Log(L"NtUserEnableIAMAccess unhooked");
        }

        HWND hwnd = FindWindowW(kOverlayClass, nullptr);
        if (!hwnd) {
            g_lastBanded = nullptr;
            continue;
        }

        DWORD band = 0;
        if (pGetWindowBand && pGetWindowBand(hwnd, &band)) {
            if (band == kTargetBand) {
                g_lastBanded = hwnd;
                continue;
            }
        } else if (hwnd == g_lastBanded) {
            continue;
        }

        if (ApplyBand(hwnd)) {
            g_lastBanded = hwnd;
            Wh_Log(L"Overlay moved to band %u", kTargetBand);
        }
    }
    return 0;
}

static bool ResolveApis() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) {
        Wh_Log(L"user32.dll not loaded");
        return false;
    }

    pSetWindowBand = (SetWindowBand_t)GetProcAddress(user32, "SetWindowBand");
    pGetWindowBand = (GetWindowBand_t)GetProcAddress(user32, "GetWindowBand");
    pNtUserEnableIAMAccess =
        (NtUserEnableIAMAccess_t)GetProcAddress(user32, MAKEINTRESOURCEA(2510));

    if (!pSetWindowBand) {
        Wh_Log(L"SetWindowBand not found");
        return false;
    }
    return true;
}

BOOL Wh_ModInit() {
    if (!ResolveApis()) {
        return FALSE;
    }

    if (pNtUserEnableIAMAccess) {
        if (Wh_SetFunctionHook((void*)pNtUserEnableIAMAccess,
                               (void*)NtUserEnableIAMAccessHook,
                               (void**)&pNtUserEnableIAMAccessOriginal)) {
            g_hookInstalled = true;
            Wh_Log(L"NtUserEnableIAMAccess hooked");
        } else {
            Wh_Log(L"Failed to hook NtUserEnableIAMAccess");
        }
    }

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        Wh_Log(L"CreateEvent failed");
        return FALSE;
    }

    g_workerThread = CreateThread(nullptr, 0, BandWorkerThread, nullptr, 0, nullptr);
    if (!g_workerThread) {
        Wh_Log(L"CreateThread failed");
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        return FALSE;
    }

    return TRUE;
}

void Wh_ModUninit() {
    if (g_stopEvent) {
        SetEvent(g_stopEvent);
    }
    if (g_workerThread) {
        WaitForSingleObject(g_workerThread, 2000);
        CloseHandle(g_workerThread);
        g_workerThread = nullptr;
    }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }

    if (g_hookInstalled && pNtUserEnableIAMAccess) {
        Wh_RemoveFunctionHook((void*)pNtUserEnableIAMAccess);
        g_hookInstalled = false;
    }

    // Best effort: return the overlay to the desktop band while it still exists.
    HWND hwnd = FindWindowW(kOverlayClass, nullptr);
    if (hwnd && pSetWindowBand) {
        if (g_iamKey) {
            pNtUserEnableIAMAccess(g_iamKey, TRUE);
            pSetWindowBand(hwnd, nullptr, ZBID_DESKTOP);
            pNtUserEnableIAMAccess(g_iamKey, FALSE);
        } else {
            pSetWindowBand(hwnd, nullptr, ZBID_DESKTOP);
        }
    }
}
