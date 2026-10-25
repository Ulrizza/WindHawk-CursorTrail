# AI Review Fixes — 2026-10-24

PR: https://github.com/ramensoftware/windhawk-mods/pull/6418#issuecomment-6389322579
Line numbers refer to the mod source at commit `2c372cc`.

No blocking issues. Both items from the previous round are confirmed fixed: the
`NtUserEnableIAMAccess` hook / borrowed key / polling are gone, and the
explorer.exe side no longer loads while Always on top is off.

## Optional improvements

- [x] 1. The band window lookup runs on every rendered frame while no usable band
  window exists (`ResolveDrawTarget`, L2255–2279). `FindBandWindow()` walks all
  top-level windows via `FindWindowExW`, up to ~125 times per second, when
  Explorer is excluded or not running, `CreateWindowInBand` failed for both
  bands, or the band rejected a blit.
  - Implemented: `ResolveDrawTarget` now rate-limits `FindBandWindow()` to once
    per second (`Runtime::lastBandLookup`), returning the own overlay in between.
- [x] 2. A band window that rejects one blit is skipped forever (`failedBand`,
  L2989), which in practice means until Explorer restarts. One transient
  `UpdateLayeredWindow` failure leaves the trail behind the taskbar for the rest
  of the session; retrying is cheap because the fallback draws in the same frame.
  - Implemented: `BlitOverlay` records when the band failed
    (`Runtime::failedBandAt`), and the rate-limited lookup stops skipping it once
    30 seconds have passed, so it is retried every 30 seconds instead of never;
    the lookup throttle keeps the polling to once a second.

## Functionality note (non-critical)

- [ ] 3. Windhawk 2.0: a tool mod can request a UIAccess host
  (`windhawk-mod-uiaccess.exe` include) and the topmost overlay would land in the
  UIAccess band by itself, so the explorer.exe part could be dropped later. 2.0 is
  still alpha; no action now.

## No action

- Code overview / side-effects sections: descriptive only.
