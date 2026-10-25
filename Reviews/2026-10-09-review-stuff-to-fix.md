# AI Review Fixes — 2026-10-09

PR: https://github.com/ramensoftware/windhawk-mods/pull/5795#issuecomment-6087647779
Line numbers refer to `mods/cursor-trail.wh.cpp` in the fork (commit 508c414).

No blocking issues — the previous round's fixes are confirmed. Only optional polish below.

## Optional improvements (decide per item)

- [x] 1. Remove the line-style cursor-image-change redraw entirely (lines 2332–2340): with a non-empty history, `BuildTrailPoints` takes the line head from `runtime.history.front()`, which only moves when a new sample is pushed; nothing in the line renderer reads the live cursor, so the redraw always produces an identical frame. The trail head reaches the new image's center via the origin glide on the next pushed sample. Remove the block and fix the misleading comment.
- [x] 2. Poll slower while a fullscreen app is detected (line 2249): the poll thread still wakes every 20 ms just to `continue` (line 2277). Give the game state a longer poll wait (`kGamePollIntervalMs = 1000`) so the game check runs once per wake (~1 s), and branch on `runtime.isGameRunning` before the hotkey/idle cases at line 2249.
- [x] 3. Clamp the trail-length settings (lines 2568–2569): `BuildTrailPoints` reserves `TrailPointBudget()` points every frame (the tail duration in ms, or the line tail length in px) and neither has an upper clamp, so a very long trail (e.g. 60000 ms) reallocates hundreds of KB per frame and an absurd value can make `reserve` throw and kill the tool process. Clamp both to a sensible maximum (and note it in their `$description`), or reserve by `runtime.history.size()` after taking the lock.
