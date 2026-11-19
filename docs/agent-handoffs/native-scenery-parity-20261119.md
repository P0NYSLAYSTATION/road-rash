# Native scenery parity — 2026-11-19

Checkout E:/road-rash, branch port/plan33b-windows, baseline82b13a1.
Original evidence is from the owner's working E:/RR/ROADRASH.EXE.

## Changes and recovered facts

Rendering now uses the live ring's BORR and DZHR placements, shared with the
collision system, instead of the scene's separately generated random placements.
Ring records retain size/mirroring and construction-time ground heights. Cached
heights, placements and inward hill band counts survive snapshot serialization
(schema3). No additional simulation RNG calls were introduced.

Original0044b630 projects static objects at half-slice depth. Ground interpolation
uses physical sub-slice offset differences, not the camera-projected bend delta.
Its signed midpoint uses SAR, and00450960/0040fef0's one-based row helpers produce
a bottom extent of exactly the texture-bound height, without an added row.
Original0044b83d..0044b8d9 rejects wholly offscreen horizontal bounds.

Original00438610 first draws DZHR types3..11, then farther actors, the remaining
DZHR types, BORR, and nearer actors. This corrects a Canyn within-slice order
mismatch. The existing fence lateral partitions remain in use.

Original00451c20 caches ground into BORR sub+8c and DZHR entry+0c. The construction
caller0045465b..00454680 completes preceding-slice heights after its next neighbour
is built, before that neighbour's split joins. Re-querying joined terrain during
drawing changes some prop heights. The native ring now retains this timing.

Correction to the preceding unfinished handoff: DZHR entry+2 is its type/mask;
0044b9c9 explicitly supplies scale8 to0044b630. It is NOT the render scale.
BORR size remains sub+62 and mirroring sub+64 bit0. DZHR mirror is entry+1 bit0.

## Measured coverage

Seven complete original component windows match native terrain/scenery vertices,
texture content, mirroring and order, with zero missing/extra/order differences:
Hiway198, Canyn302, City236, Medly268, Napa149, Napa split2864:522, split4514:301.
Total1976 nodes. Comparator imports original world route/terrain/placement and
cached-ground inputs; it does not import projected scenery vertices. This is
component construction evidence, not complete native race-frame parity or proof
of all live placement/cache histories. Before using original cached-ground inputs,
all five ordinary course windows also matched using fresh native ground queries;
split windows exposed the cache-timing difference.

Complete-list actor recording now stores absolute indices across internal flushes
and selected actor identities. Synthetic forwarding/saturation/flush/clipping tests
pass with assertions enabled in Release. Existing natural Napa captures match16
nodes in one frame and56 nodes across four sampled Level5 frames0/60/120/180.
This is short selection/projection coverage, not a whole-race actor comparison.

## Verification

Full Release build: build/scenery-parity-full-build2.log. The first full build
hit an executable file lock because its comparator was concurrently running;
rerun after the comparator exited completed successfully. One diagnostic build
failed for missing std::cerr include; replaced diagnostic with existing stdio,
then removed temporary diagnostics. One focused target command used a hyphen
instead of the declared underscore; corrected without changing test scope.

Seven original scenery gates pass (build/scenery-original-gate.log). Focused
23-test gate initially passed22 and failed race-cache because its direct reference
renderer omitted the live ring. It now receives the same live ring as gameplay;
cache, track_ring and oracle-actor-record rerun all pass. No golden was changed.
Snapshot/native checks explicitly compare populated cached heights/render flags
and inward bands, followed by deterministic restored continuation.

Final focused gate:25/25 pass, no skips (build/scenery-final-focused-gate.log).
Strict five completed human races:143764ticks identical including RNG, single
anchor, no skips/divergences/failures (build/scenery-five-finishes.json).
The updated32-bit capture DLL also builds successfully
(build/scenery-oracle-dll-build.log); it was not installed/run for this slice.

Commands:
- tools/build-local.ps1 -Configuration Release
- ctest --test-dir build/release --output-on-failure -R
  '^(oracle-(actor-record|terrain-window|terrain-split|static-window|static-split)|snapshot$|snapshot-native$|track_ring$|scenery$|race-cache$|race-actor-placement$|race-rider-reactions$|race-live-splits$|race-start-camera$|race-crest-canvas$)'
- tools/verify_sim.py --mode acceptance --whole-race --driver human, using the
  five accepted captures retained in the C-worktree analysis/oracle directory,
  E:/RR assets and build/release/oracle-sim-test.exe; report above.


## Remaining work

Full native race-frame parity remains open. Native scenery component tests use
original world inputs; original/native end-to-end placement and cache histories,
combined live actor/scenery ordering, and full composed natural-race rendering
still require targeted numerical evidence. No menu or widescreen-only work.
No visual inspection of every race frame. Do not execute the installer.
