# Starting-grid camera handoff — 2026-11-14

Baseline: Windows branch `port/plan33b-windows`, `5361b6e`.

## Reproduction and original evidence

A loaded Game with no input held its constructor camera throughout the 180-tick
wall-clock countdown, then corrected the player's lateral placement and camera
on its first simulation tick. At level 5, City camera x jumped 48 -> 261 and
Medly 48 -> 444; level 1 showed smaller City 48 -> 73, Medly 48 -> 69 and
Napa 48 -> 54 changes. Height remained 150 and longitudinal position 12544.
These are native measurements with the supplied E:/RR assets, not live original
captures. `build/start-camera-before.log` preserves the failing-before values.

Read-only disassembly of the working `E:/RR/ROADRASH.EXE` at 004163d0 proves:
- 004163e7 initializes the race/countdown via 00442dd0.
- 004163f3 loads ECX=1; 004163f5 calls driver 0044b2e0(1).
- 004163fa draws that settled scene via 004439f0.
- 004163ff again loads ECX=1; 00416401 calls 0044b2e0(1).
- 00416406 draws again, before the visible fade at 00416434.

Thus the original has two zero-input setup simulation steps before showing the
starting grid. The examined chase-camera routine 00437c30 applies its spatial
follow law directly; no countdown-to-gameplay temporal smoothing was found.
Game::restart omitted both steps, exposing startup placement correction at GO.

## Change and verification

Game::restart now runs the two original zero-input steps immediately after the
fully wired race is built, before initializing presentation history/countdown.
Initial/restarted visible races consequently begin at simulation tick 2.
The raw build_race factory still returns tick 0 for oracle replay consumers.
No camera interpolation law, rider physics, random reanchoring or golden fixture
was changed. This change applies to the local Game startup path.

Registered `race-start-camera` exercises all five courses at levels 1 and 5,
including the full wall-clock countdown and first zero-input control tick.
All ten keep x, height and longitudinal camera position identical across the
handoff. City level 5 now starts/stays at (261,150), Medly at (444,150).

MSVC Release `race_screen-test` builds. Focused CTest
`^(race-screen|race-start-camera|race-crest-canvas)$` passes all three tests,
zero failures/skips (29.03 seconds). Existing screen timing tests measure elapsed
ticks from the new two-step setup baseline, including 30/60/120 FPS equivalence.
The nitro test now holds boost for eight drain ticks before releasing it and
checks the consecutive modulo recharge sequence: activation alone need not have
spent accumulator charge at the changed scheduler phase. Actual charge drain
and inactive/partial-charge preconditions are asserted; no check is skipped.

Logs: `build/start-camera-build.log`, `build/start-camera-before.log`,
`build/start-camera-tests.log`. No original-game launch, desktop replay capture,
full-race replay or installer upgrade run was performed for this startup fix.
