# AI Review Fixes — 2026-10-08

PR: https://github.com/ramensoftware/windhawk-mods/pull/5795
Line numbers refer to `mods/cursor-trail.wh.cpp` in the fork (commit 4f45f48).

## Required fixes

- [x] 1. Hide overlay only when the last frame drew nothing — `EnterIdleIfInactive` (line 2223): guard `ShowWindow(SW_HIDE)` with `!runtime.hasPrevDirty` so a static trail stays visible while the timer is stopped
- [x] 2. Drop stale `runtime.cursorHidden` — use `sampleCursor == NULL` at lines 2312 and 2365 (already computed at line 2272)
- [x] 3. Detect fullscreen on the poll thread (every 500 ms) and `MarkContentChanged` on change; remove the check from `SmearTimerProc` (lines 2889–2893) and the two `if (!gameRunning)` guards (lines 2937, 3088) so idle engages during games

## Optional improvements (decide per item)

- [x] 4. Longer poll wait while the trail is off via hotkey (line 2260), e.g. 250 ms
- [x] 5. Guard line-style cursor-image `MarkContentChanged` with `!runtime.history.empty()` (line 2303)
- [x] 6. Rename leftover window title `L"SmearOverlay"` (line 3207)
- [x] 7. Remove redundant `SetThreadDpiAwarenessContext(...)` calls (lines 2245, 3184)
- [x] 8. Merge README "How it differs" into "The two styles" (renamed to "Styles")

## README / docs

- [x] 9. Sync `README.md`, the `==WindhawkModReadme==` block, and settings metadata if any doc-affecting change lands

## Post-review changes (2026-10-08)

- [x] 10. Clear the trail immediately when the pointer is hidden (Windows' hide-while-typing) — `PollThreadProc` `!sampleCursor` now flushes `runtime.history` + `MarkContentChanged()` instead of fading over the tail duration; removed the redundant hidden clause from the size-based timeout condition and the old fade-out hidden block
  - Investigated a report that the pointer "reappears" ~1 s after hiding while typing. Traced with logging: the mod is fully idle and performs no action at the moment the pointer returns, and the pointer also returns with the mod disabled. It's the app/OS "hide pointer while typing" re-show behavior, not a mod bug. Only the trail-flush (above) was kept; no other change.
