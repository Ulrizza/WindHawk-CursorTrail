# AI Review Fixes — 2026-10-16

PR: https://github.com/ramensoftware/windhawk-mods/pull/6292#issuecomment-6222350203
Line numbers refer to the mod source at commit `13df1e5`.

One required item; the rest are optional or non-critical notes. Good shape
otherwise: worker thread joined in `Wh_ModUninit`, nothing written to disk, band
change undone on unload.

## Required fix

- [x] 1. Hook operations from the worker thread can run after Windhawk has removed the mod's hooks (lines 116–134, 143–148, 173–179). Fixed by keeping the hook installed for the mod's whole lifetime (removed `ArmIamHook`, `g_hookInstalled`, `g_unhookPending`, the worker's remove block and the runtime re-add; the hook now records the latest key).
  - Per `windhawk_api.h`, `Wh_ApplyHookOperations()` "can't be called before `Wh_ModInit` returns or after `Wh_ModBeforeUninit` returns", and it is "very slow, avoid using it if possible".
  - The mod has no `Wh_ModBeforeUninit`, and the worker runs until `Wh_ModUninit` signals it. So a hook remove/re-add can run after Windhawk already removed the mod's hooks. A hook re-added at that moment can stay patched into `user32` after the mod DLL is unloaded → the next `NtUserEnableIAMAccess` call (e.g. pressing Win) makes Explorer jump into unmapped memory and crash.
  - Suggested fix: keep the hook installed for the mod's whole lifetime and drop the add/remove machinery — remove `g_hookInstalled`, `g_unhookPending`, the remove block in the worker (lines 173–179) and the runtime re-add, which also removes the last `Wh_ApplyHookOperations()` call. A dropped key is recaptured by itself the next time the shell enables IAM access:
    ```cpp
    static BOOL WINAPI NtUserEnableIAMAccessHook(ULONG64 key, BOOL enable) {
        BOOL result = pNtUserEnableIAMAccessOriginal(key, enable);
        if (result && key) {
            g_iamKey.store(key);
        }
        return result;
    }

    // Wh_ModInit:
    if (pNtUserEnableIAMAccess) {
        WindhawkUtils::SetFunctionHook(pNtUserEnableIAMAccess,
                                       NtUserEnableIAMAccessHook,
                                       &pNtUserEnableIAMAccessOriginal);
    }
    ```
  - Alternative (if the runtime add/remove is kept): add a `Wh_ModBeforeUninit` that signals the worker and joins it, so no hook operation can run after that callback returns.

## Optional improvements

- [x] 2. Add a visual to the README: added before/after GIFs (`images/AlwaysOnTopBefore.gif` and `images/AlwaysOnTopAfter.gif`) to the description, referenced by their absolute raw URLs.
- [x] 3. README step 3 doesn't match the code (line 37): it said the hook is installed only "if that is denied", but `Wh_ModInit` always installs it. Reworded to say the hook is always installed and records the key the shell uses.
- [ ] 4. Check that the window found is really the trail overlay before raising it (lines 181–200) — today any top-level window with the class `CursorTrailOverlayClass` is raised into `ZBID_SYSTEM_TOOLS`, including an unrelated app using the same generic class name or a sandboxed window. Two cheap checks:
  - Only raise click-through windows: `GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT`.
  - Check the owning process with `GetWindowThreadProcessId` + `QueryFullProcessImageNameW`. Simple Cursor Trail runs in `windhawk.exe`, which becomes `windhawk-mod.exe` in Windhawk 2.0.

## Functionality notes (non-critical)

- [x] 5. The key is dropped on every `SetWindowBand` failure (lines 143–148). Superseded by fix #1: the key is no longer dropped at all, and the always-installed hook overwrites it with the latest key.
- [ ] 6. `NtUserEnableIAMAccess` is resolved by the hard-coded ordinal 2510 (line 221) with no verification. On a build where that ordinal is a different export, the mod hooks an unrelated function and calls it with `(key, TRUE/FALSE)` inside Explorer. Consider verifying the address (e.g. compare it against `win32u.dll`'s `NtUserEnableIAMAccess` named export on builds that have it).
- [ ] 7. Every `explorer.exe` process runs the loop (lines 166–209). When folder windows run in a separate process, those instances also get the mod, never capture the shell's key, their direct `SetWindowBand` calls fail, and they keep polling every second. Skip the work in any process that doesn't own `Shell_TrayWnd` (compare `GetWindowThreadProcessId(FindWindowW(L"Shell_TrayWnd", nullptr), &pid)` with `GetCurrentProcessId()`).

## No action

- Keeping this as its own mod (rather than an option in Simple Cursor Trail) is correct: the main mod is a tool mod running in `windhawk.exe`, and only Explorer has the IAM key `SetWindowBand` needs.
