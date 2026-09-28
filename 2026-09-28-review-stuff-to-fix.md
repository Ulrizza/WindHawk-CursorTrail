# AI Review Fixes — 2026-09-28

PR: https://github.com/ramensoftware/windhawk-mods/pull/5795
Line numbers refer to `mods/cursor-trail.wh.cpp` in the fork.

## Required fixes

- [x] 1. Remove `timeBeginPeriod(1)`/`timeEndPeriod(1)` from launcher boilerplate (lines 3240, 3390) — matches wiki snippet verbatim
- [x] 2. Add idle state — stop multimedia timer + `timeEndPeriod(1)` when idle, longer poll timeout (`20` ms when idle)
- [ ] 3. Fix screen-sized bitmap leak in `EnsureBackbuffer` (lines 2386–2388) — `DeleteDC` before `DeleteObject`
- [ ] 4a. Resolve companion-mod references in README (submit helper separately, or remove references)
- [ ] 4b. Rename window class `SmearFrameOverlayClass` → unique name (e.g. from `WH_MOD_ID`)
- [ ] 5a. Credit Cursor Motion Blur / TheatriChris (MIT license notice)
- [ ] 5b. Add README sentence explaining difference vs Mouse Trail + Cursor Motion Blur
- [ ] 5c. Consider a more distinctive mod name

## Optional improvements (decide per item)

- [ ] 6. Load settings on overlay thread (fix settings↔render/poll race) — `kMsgApplySettings` under `historyMutex`
- [ ] 7. Drop `TerminateThread`; simplify shutdown (post to `overlayHwnd`, `INFINITE` join, remove `timeKillEvent`+`Sleep`)
- [ ] 8. Remove dead debug code (`debugShowOutline`/`debugShowTailPreview`, `DrawDebug`, `RenderTailPreview`, debug brushes)
- [ ] 9. Don't hold `cursor.offsetMutex` during `ComputeCursorGeom`; create one shared `IWICImagingFactory`
- [ ] 10a. Move "Architecture" section out of catalog README (→ GitHub repo)
- [ ] 10b. Fix relative `[LICENSE](LICENSE)` link (won't resolve in Windhawk)
- [ ] 10c. Fix typos: "Disclamer", "botherded" (line 26)
- [ ] 11. Fix `settings.sizeTimeout` DWORD negative check (line 1203)
- [ ] 12. Reword profane comment inherited from Cursor Motion Blur (line 3068)

## Functionality notes (non-critical ideas)

- [ ] 13. Render only trail bbox, not the whole virtual desktop
- [ ] 14. Hide overlay window when idle
- [ ] 15. Refresh cursor geometry on per-monitor DPI change
- [ ] 16. `IsGameRunning` stale `Progman`/`WorkerW` handle cache
