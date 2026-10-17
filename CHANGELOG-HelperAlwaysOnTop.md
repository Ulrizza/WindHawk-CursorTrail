# Changelog — Simple Cursor Trail helper - always on top

## 1.4
- Resolve and hook `win32u!NtUserEnableIAMAccess` by name instead of verifying
  user32's ordinal-2510 thunk with an x86/x64-only opcode scan, so the IAM key
  hook works on ARM64 too (and also catches direct win32u imports). Removed
  `ThunkCallsTarget` and the ordinal lookup, and dropped the matching note from
  the description.

## 1.3
- Kept the `NtUserEnableIAMAccess` hook installed for the mod's whole lifetime
  instead of adding/removing it at runtime, so no `Wh_ApplyHookOperations` call
  can run after Windhawk has removed the mod's hooks (first AI-review fix). The
  hook now records the latest key, which also self-heals a stale one; the README
  step describing the hook was updated to match.
- Added before/after visuals to the description
  (`images/AlwaysOnTopBefore.gif`, `images/AlwaysOnTopAfter.gif`).
- Only raise the overlay after verifying it is click-through and owned by
  `windhawk.exe` (`windhawk-mod.exe` on Windhawk 2.0), so an unrelated window
  with the same class name or a sandboxed window is never lifted above the shell.
- Skip the worker in `explorer.exe` instances that don't host `Shell_TrayWnd`
  (e.g. separate-process folder windows).
- Only hook `NtUserEnableIAMAccess` after verifying that user32's ordinal 2510
  forwards to `win32u.dll`'s named export; otherwise the IAM hook is disabled so
  a changed ordinal can never make the mod hook an unrelated function.

## 1.2
- Renamed the mod from **Cursor trail helper - always on top** to **Simple Cursor
  Trail helper - always on top**, to match the main mod's name.
- Clarified the description's opening with a plain-language summary of what the
  mod does, before the Z-order-band explanation.

## 1.1
- The worker poll now backs off to a 5 s interval once the band is confirmed
  (1 s while hunting), instead of waking every second for the whole session.
- `Wh_ModUninit` joins the worker with `INFINITE`, so the stop event and thread
  handles are never closed while the worker can still be running.
- If a captured IAM access key fails, it is dropped and the
  `NtUserEnableIAMAccess` hook is re-armed so a fresh key can be captured.
- `g_hookInstalled` is now atomic and the unused `ZBID_IMMERSIVE_MOGO` enum
  value was removed.
- Added a `## Licence` section and a link to the Simple Cursor Trail mod to the
  description.

## 1.0
- Added the **Simple Cursor Trail helper - always on top** companion mod
  (`CursorTrailHelperAlwaysOnTop.cpp`, mod id
  `cursor-trail-helper-always-on-top`). It runs inside `explorer.exe` and moves
  the overlay into `ZBID_SYSTEM_TOOLS` via the undocumented `SetWindowBand` API,
  so the trail draws above the Windows 11 taskbar and Start menu. When direct
  banding is denied it captures the IAM access key by hooking
  `NtUserEnableIAMAccess` (unhooked once captured), and re-applies the band if
  it is reset or the overlay is recreated.
- Made the cross-thread `g_iamKey`/`g_unhookPending` state `std::atomic` (was
  `volatile`).
- The runtime unhook now calls `Wh_ApplyHookOperations()` so it actually takes
  effect.
- Removed the redundant, out-of-window `Wh_RemoveFunctionHook` call from
  `Wh_ModUninit`.
