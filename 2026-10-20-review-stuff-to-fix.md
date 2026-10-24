# AI Review Fixes — 2026-10-20

PR: https://github.com/ramensoftware/windhawk-mods/pull/6418#issuecomment-6277567988
Line numbers refer to the mod source at commit `dc278c1`.

The rendering side is unchanged. Both required items concern the new explorer.exe
part of Always on top.

## Required fixes

- [x] 1. Replace the `win32u!NtUserEnableIAMAccess` hook / borrowed IAM key / polling
  with `CreateWindowInBand`, as in Pointer Afterimage Motion Blur, or justify in the
  PR why `CreateWindowInBand` did not work.
  - Sub-points: create the band window from a mod-owned thread with its own message
    loop in shell explorer.exe (layered, click-through; try `ZBID_UIACCESS`, fall back
    to `ZBID_SYSTEM_TOOLS`); point `BlitOverlay` at it; have the tool process draw into
    it with `UpdateLayeredWindow`; hide it when the tool process exits by watching the
    process handle; use `ShowWindowAsync` / `SetWindowPos(..., SWP_ASYNCWINDOWPOS)` for
    idle hide/show; keep the existing overlay window for the hotkey, timer and fallback.
  - This removes the syscall hook, borrowed key, 1 s/5 s polling and the "press Win
    once" step. Current code: hook L3673–3681, key storage L3441–3447, worker L3569–3617.
  - Implemented: explorer.exe now only runs `BandThread`, which creates a 1×1 layered,
    click-through `CreateWindowInBand` window (UIAccess then system-tools) and watches
    the tool process handle. The tool finds the band window, draws into it with
    `UpdateLayeredWindow`, falls back to its own overlay on failure, and uses async
    `SetWindowPos` for the cross-process idle hide/show. The hook, key, polling and
    `SetWindowBand` code are gone; README/description were updated.
- [x] 2. Make the setting actually unload the explorer.exe code when off.
  - `Wh_ModInit` explorer branch: `if (!IsShellExplorer() || !Wh_GetIntSetting(L"alwaysOnTop")) return FALSE;`
  - `Wh_ModSettingsChanged`: in explorer set `*bReload = !Wh_GetIntSetting(L"alwaysOnTop")`;
    the old instance's `Wh_ModUninit` stops `BandThread` (destroying the band window) first.
  - Remove `alwaysOnTop.enabled`, `AlwaysOnTopSettingsChanged`, and the worker's
    "turned off" branch (L3577–3588). These are already gone with item 1; only the
    init gate and the reload variant remain.
  - Implemented: `Wh_ModInit` rejects the explorer side while the setting is off,
    `Wh_ModSettingsChanged` uses the `BOOL* bReload` variant and asks for a reload in
    explorer when the setting is turned off, and `Wh_ModUninit` stops `BandThread`
    before the reload. The tool process is unaffected and falls back on its own.

## Optional improvements

- [x] 3. Remove polling (only if the current approach is kept): signal `alwaysOnTop.wakeEvent`
  from the hook on a new key, and signal from the tool process once its overlay exists
  (per-session named event or `RegisterWindowMessage`), then wait with `INFINITE`.
  - Resolved by item 1: the IAM hook and polling worker are gone; `BandThread` blocks
    on its message queue and the tool process handle instead.
- [x] 4. Remove `g_isToolMod`; the `|| !g_isToolMod` checks in `Wh_ModSettingsChanged`
  and `Wh_ModUninit` (L3896, L3909) can never be true.
  - Implemented: removed the flag and its assignment; both callbacks now test only
    `g_isToolModProcessLauncher`.
- [x] 5. Fix the misleading comment at L3466 ("try without a key first"); the direct
  `SetWindowBand` call only runs when no key was captured.
  - Resolved by item 1: `ApplyWindowBand` and the comment were removed with the
    `SetWindowBand` code.
- [x] 6. Credit Pointer Afterimage Motion Blur in `README.md`, next to the existing
  Cursor Motion Blur credit, for `IsShellExplorer` and the explorer.exe init branch.
  - Implemented: added the credit paragraph to both `README.md` and the mod
    description, covering the band window, its process-exit watch and `IsShellExplorer`.

## Functionality note (non-critical)

- [ ] 7. Windhawk 2.0: consider the UIAccess host (`windhawk-mod-uiaccess.exe` include)
  so the explorer.exe part can be dropped later; 2.0 is still alpha.

## No action

- Code overview / side-effects sections: descriptive only.
