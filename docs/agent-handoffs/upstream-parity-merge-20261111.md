# Upstream parity merge — November 11, 2026

- Fork baseline: 1d575f0, codex/simulation-parity.
- Merged upstream main: 20fb4c0ac154378902132f6eb41a5e09804ebeaa (23 commits).
  Upstream exposes no separate simulation-parity branch; these fixes are on main.
- Includes dual-half track-ring/physical-road geometry, City pedestrian draw
  gating and obstacle scans, human scheduler cadence, remount copy-back,
  crossing-car scan limits and crashed-cop animations. Read the new upstream
  handoffs before selecting further parity work.
- Retained fork-specific Windows behavior: XInput/DirectInput with startup-only
  discovery, complete SDL scancode persistence, separate Turn Left/Right and
  Lean Left/Right rows, built-in soundfont MIDI at Mixer::rate, installer and
  DirectX presentation. Upstream original joystick tables/paired capture were
  not reactivated. Upstream font/image/SF1 improvements and their dependencies
  were integrated around those choices.
- Unfinished local graphics-lifetime experiment was preserved before merging
  at artifacts/upstream-merge-20261111/unfinished-family-lifetime.patch and
  copies of its two source files. It is not reapplied: reassess it against
  the updated implementation. No broad stash/reset was used.

## Verification

Full Debug build succeeds after conflict resolution. Initial CTest run found
one input test mismatch because upstream tests expected original encoded
keyboard tables; those tests and the fork's full-scancode implementation
were restored together. Final gate PASS: 138 registered, 102 passed, 36 explicit corpus/platform
skips, zero failures (build/upstream-merge-final-tests.log).
Napa receipted human capture: build/upstream-merge-napa.json PASS,
2400 ticks identical, seed/first-frame RNG verified with one anchor.
Expanded City capture still stops at tick 514 on pedestrian graphics eviction
(build/upstream-merge-city.json), matching the pre-merge open failure.
This is an integration check, not full simulation parity acceptance.

## Next work

Continue parity before multiplayer. Upstream did not implement graphics-family
replacement/eviction: +1e0 can clear while old bank pointers remain. Current
local moving-player expanded captures expose it (Napa901, City514). Original
family-slot diagnostic capture city-family-lifetime-600 shows slot0 changing
10->12 at frame505/front sample165, then pedestrian6 clears +1e0 on its next
think514. MSRR family bias is -28; front165-28 reaches load event137. Use that
measurement and new upstream docs rather than copying old prototypes blindly.

No reference executable/DLL install is used; only E:/RR is captured. Commercial
traces stay local. No original game process or capture is left running by this
merge. Multiplayer edits in E:/road-rash are untouched.
