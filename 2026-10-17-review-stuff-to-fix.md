# AI Review Fixes — 2026-10-17

PR: https://github.com/ramensoftware/windhawk-mods/pull/6292#issuecomment-6240448630
Line numbers refer to the mod source at commit `f3630f0`.

The previous round's findings are all fixed. One item is left, in the ordinal
check; the rest are optional notes.

## Required fix

- [x] 1. The ordinal check only understands x86/x64 code, so the IAM hook is likely disabled on ARM64 (lines 258–282, 294–317). `ThunkCallsTarget` scans user32's ordinal 2510 for x86/x64 `call/jmp [mem]` opcodes (`FF 15` / `FF 25`). The mod has no `@architecture`, so on ARM64 it runs natively in Explorer, where an import thunk is an `adrp` / `ldr` / `br x16` sequence; the scan can never match, `pNtUserEnableIAMAccess` stays null, the IAM hook is never installed, and the trail silently stays under the taskbar (the direct `SetWindowBand` call is denied).
  - The check isn't needed: `ResolveApis` already requires `win32u!NtUserEnableIAMAccess` by name, which is where the user32 thunk ends up. Hook and call the win32u export directly (as `taskbar-thumbnails` does for `NtUserShowWindow` / `NtUserEnableWindow`). Works on every architecture and also catches callers that import from win32u directly.
  - Suggested `ResolveApis()` replacement for lines 294–317:
    ```cpp
    HMODULE win32u = GetModuleHandleW(L"win32u.dll");
    pNtUserEnableIAMAccess = win32u
        ? (NtUserEnableIAMAccess_t)GetProcAddress(win32u, "NtUserEnableIAMAccess")
        : nullptr;
    if (!pNtUserEnableIAMAccess) {
        Wh_Log(L"win32u!NtUserEnableIAMAccess not found, IAM hook disabled");
    }
    ```
  - Delete `ThunkCallsTarget` and the ordinal lookup.
  - Remove the README note about verifying the ordinal (description lines 57–59); with this change it no longer applies. The root `README.md` documents the main mod, not this helper.
  - Implemented: `ResolveApis` now resolves `win32u!NtUserEnableIAMAccess` by name and hooks that directly; `ThunkCallsTarget` and the ordinal 2510 lookup are gone, and the note was removed from the description block.

## Functionality notes (decide per item)

- [x] 2. A failing captured key is retried every second and logged each time until the shell uses a new key (lines 125–136). Harmless; a fresh key replaces the old one automatically. Likely no action, just noted.
  - No action: retrying is required so a recreated overlay is raised without waiting for a new key; the cost is one wakeup and one log line per second, accepted. Verified the hook always stores the latest key (`g_iamKey.store` in `NtUserEnableIAMAccessHook`), so the stale key self-heals.
- [x] 3. Separate mod vs. an option in Simple Cursor Trail: keeping it separate is correct (the main mod runs in `windhawk.exe`; only Explorer has the IAM key `SetWindowBand` needs).
  - No action: confirmed the main mod is a tool mod (`@include windhawk.exe`, `WhTool_ModInit` launcher at CursorTrail.cpp:3312,3373) and the helper must run in `explorer.exe`, which is where the shell's IAM key lives.

## No action

- Code overview / side-effects sections: descriptive only, nothing to change.
