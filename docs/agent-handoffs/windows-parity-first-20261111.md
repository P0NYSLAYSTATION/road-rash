# Deterministic parity first — 2026-11-11

Multiplayer implementation is on hold at the owner's request. The unrelated
snapshot changes remain in E:/road-rash; this checkout is based on 05d6c31 on
codex/simulation-parity and contains capture/replay and parity fixes only.

## RNG audit

The original hook pins RR_ORACLE_SEED before race setup. The native replay
initializes the same MSVC stream (state * 0x343fd + 0x269ec3), including setup
consumers, then free-runs. Strict runs do not use --reanchor.

City seed 1: native factory completion is three draws behind the original's
first captured frame. This compares different boundaries: the original frame
is POST tick 1. A caller trace identifies the three extra draws at 0x440eff
(pedestrian countdown setup). After executing the same native countdown tick,
both states are exactly 742760408. The replay now explicitly checks this
first-frame state before comparing spawn fields. It checks green closure and
every subsequent tick's RNG as well. Evidence: build/city-rng-audit.json,
build/city-curvature-parity.json, analysis/oracle/city-setup-rng-4.tsv.

## Measured changes and current failures

- Traffic linked slots at/beyond the front still hold their previous lap.
  Fixing this makes the historical 600-tick Napa capture match all compared
  fields, including 23 previously unverified diagnostics.
- Car pool reuse preserves +108/+10c velocity until movement. Original
  0x4387a0 resets speed and the explicit initialization words, not velocity.
  Napa tick 1780 reuses the slot whose old velocity is 1540. The native
  initialization now preserves the motion image and resets the actual words.
- AI curvature lookahead (0x449b40) follows those circular slots too. City
  grid sample 123 looks at 124..129, then old slots 2 and 3. Reading future
  course data incorrectly included a bend and shifted the grid by 512 Q8.
  This fix yields matching City grid/countdown and RNG through tick 183.
- Current City first divergence: missing native RNG draw at tick 184;
  original caller 0x4410ba in pedestrian crossing logic. Investigation ongoing.
- Current Napa 2400 first divergence: tick 1826, ring car previous linked
  lane width original 257 versus native 256. Road-walker investigation open.

Native captures use only E:/RR/ROADRASH.EXE and an instrumented copy of that
installation's AWEMAN32.DLL, restored with checksum verification after each
capture. Commercial traces and binaries remain local and uncommitted.

## Validation

MSVC Debug oracle, bike-test and traffic-test built successfully. bike-test:
0 failures. traffic-test synthetic checks pass; disc corpus portions skipped
without corpus configuration. New regressions cover preserved recycled
velocity and the unbuilt forward ring link. Full integration gate pending.
The native Windows capture adds measured human-player branch invocation
counters; the old 600-tick capture lacks these and cannot pass modern strict
human acceptance even though its state comparison is exact.

No full-course, full-race or overall deterministic parity completion is claimed.
No parity commit, push, installer or release has been made yet.


## Later update

Napa human 2400 strict acceptance now PASSES (build/napa-pool-order.json).
The lane mismatches were original widening nudges in 0x4547e0, not lane
interpolation. Pool allocation is LIFO; simultaneous returns follow ascending
position. Added regressions for both. Full integration initially failed only
a new test with incorrect synthetic setup; that was corrected and focused
traffic passes. Formatted full rebuild and final full gate currently pending.
Expanded City now stops at tick 514 on original +1e0 graphics eviction:
0x44143b tests the RRAN header, 0x44145f clears the loaded byte and animation
descriptor while old bank pointers remain. Native currently conflates cached
look/bank and loaded graphics. No eviction fix has yet been implemented.
Pedestrian graphics loading was restricted to the draw horizon; this and the
serial-counter reset (0x442a3d -> 0x44f460) improve expanded City comparison.
Next: port family/graphics lifetime exactly; refresh expanded Napa captures
with pedestrian fields, then additional courses/levels/combat/full-race work.


Final gate for this slice: full Debug build and CTest succeed, 138 registered,
102 passed/36 corpus or platform skips/zero failed. New pool-return regression
and corrected widening fixture pass. Python capture tools compile successfully.
Expanded Napa capture (with pedestrian state, bank presence and load flag)
stops at tick 901 on the same resource-eviction issue. Older Napa2400 strict
pass does not cover these new fields. Current family investigation has
read-only original MSRR walker disassembly in build/original-family-walker.txt.
0x452310 uses mode byte +0x10 and distance global 0x476cd8; globals 0x4b7f10
hold current family IDs per slot. The latest diagnostic helper logs these
with RR_ORACLE_TERRAIN_TRACE=1. city-family-lifetime-600 is the current capture.
Do not weaken the resource comparisons to make expanded acceptance green.
