# Native composed rendering — 2026-11-19

Checkout E:/road-rash, Windows branch port/plan33b-windows. Baseline bca710f;
release1.1.32 published. Standing authorization for builds/commit/push/releases.
Only original E:/RR/ROADRASH.EXE; no installer execution, menu pixel work,
widescreen-only work, visual inspection of every frame or extra branches.

## Completed in-flight implementation

Original00438509 copies a pedestrian's current animation block+70 to its drawn
block+8c before00440680 lazily loads category2 graphics. The original first draw
can initialize the current block while the copied selection+a8 remains zero.
The native renderer incorrectly used the newly selected current animation.
Pedestrian now retains an optional draw_animation copied before lazy loading;
pedestrian_node uses it. Standalone fixtures without a frame cache retain their
explicit current selection. Snapshot schema4 serializes this presentation state.

Original0044ba62..0044ba8e replaces RRAN width/height with indexed texture bounds.
Native actor projection used nominal dimensions instead, extending a pedestrian
one cell in width/height. Both axis and rotated projection now use indexed bounds;
Bob sheets have no frame.texture and keep their nominal RRAN dimensions.

## Evidence and test setup

New original capture analysis/oracle/napa-seeded-render-20261119.tsv:240contiguous
human ticks, seed1 applied before setup, seven full draw lists at render frames
0/60/120/180/240/300/360 (simulation ticks1/2/2/12/75/140/205). Complete receipt
confirms the working DLL was instrumented in place and restored. No other install.
SIM0 natural captures used earlier did not apply the requested seed, so they were
not used to claim native setup/RNG generation fidelity.

New oracle-sim-test --render-ring / --render-scene replays the real factory and
controls from one anchor. Uses raw scene original global tick004b7ecc for matching.
--render-ring compares generated BORR/DZHR counts, size/mirror, lateral placement
and cached heights against original, importing none of those inputs.
--render-scene also borrows the same native RaceInstance/assets into Game under
RAII restoration, calls the real actor emitter and compares the complete composed
list (coordinates, flips, texture/frame/palette identity and draw order). Only the
original viewport position/X/height/half are imported. This isolates presentation
from camera interpolation; it does NOT establish native-camera or pixel parity.
Dynamic Bob nodes have no static hash: compare their actual frame/palette resource
identity rather than falsely treating a zero hash as a rendering failure.

Napa now matches7windows,224generated placements/cached heights,1869composed nodes.
The240tick simulation matches compared fields and RNG; report separately the
one input metadata field and6nondeterministic sound-thread RPM timing notes.
Do not label that short capture strict whole-race acceptance.
Logs: build/napa-native-composed-scene4.log; initial failures/probes retained in
build/napa-native-*.log. No golden or expected projected geometry changed.

CMake registers oracle-render-native-napa. Original indexed/Bob dimension synthetic
check added to actor_render. Nine focused checks pass. Expanded32-check gate
passed31 but snapshot-native's new coverage assertion found no naturally selected
pedestrian at its fixture location. The fixture now explicitly seeds a selected
draw cache in an inactive slot; both snapshot checks pass, including120ticks of
restored continuation. Not a disabled assertion or changed gameplay expectation.
Final expanded gate and strict five finished-race check remain pending below.

## Highway finish capture and current diagnosis (November 20)

The capture completed: analysis/oracle/hiway-native-render-finish-20261119.tsv,
20710 contiguous ticks, human/seed1/level1, race_finished true/outcome3.
Working E:/RR DLL was restored; no original process/capture remains active.
Drive5 used the five-second stuck detector/recovery; no new original capture is
needed to finish comparison of this retained recording.

Native composed replay uncovered and fixed, in order:
- Tick10104: nonpositive-countdown hill joins omitted the physical section offset
  difference in branch separation. Positive countdown shoulder clamping still
  uses the integrated separation alone, as original0045495d.
- Split pedestrians were emitted on the player's road instead of their physical
  roadway. Active pedestrian membership/projection now use the correct half.
- Tick10712: discarded branch's terminal prop grounds must retain the physical
  slot cache instead of re-evaluating a self-neighbour.
- Tick12537:004546d0 carries the preceding hill outline across a marked first
  section slice and flattens its heights. Cached prop ground is queried BEFORE
  this continuity update, against the physical slot's retained hill union fields.
  Shoulders.raw_ground retains those values and snapshot schema4 serializes them.
- Six SIDLANE0 nodes:0043b94e uses the shared side-street marker to select8/16
  tiles; terrain kind alone is insufficient. build/hiway-native-marked-streets.log
  then reached tick16175, frame16200 before its first draw-list mismatch.

The remaining vertex was2087 Q16 units wrong, exactly one world unit at scale2087.
Original half1 sample1452/local112 shoulder-left is140; a precomputed FORWARD
DLSR walk produces139. The original branch was initialized backwards and retains
its reverse Q16 fraction. Independent integer modelling against the retained
original scene matches ALL10 half1 shoulder widths in that window, and accounts
exactly for native bad vertex21063144 versus original21061057.
Receipt: build/hiway-reverse-shoulder-proof.json. This is a numerical model,
not by itself C++ runtime acceptance.

Implemented CourseSection.side_intervals plus SideCursor init/step/apply using
00452a00/00452b70/00452b00. Each SectionWalker owns its Q16 side cursor, including
cloned split/merge windows and direction reversals. path_of overlays live widths,
heights and flags over the immutable route sample. Schema4 also saves these
cursor fields. Synthetic course tests cover opposite-end rounding, reversal,
sub-sample movement and copied-state continuation. Boundary tests exercise marked
entry/exit, coordinate translation, flattened heights and ordinary boundaries.
The composed replay now checks original/native live shoulder widths explicitly.

Current targeted build/replay: build/reverse-side-iterator-build.log and
build/hiway-native-reverse-sides.log; tool session47768 (check before running
another build). This section supersedes the stale active-capture text above.

## Remaining validation and delivery

Earlier32-check focused gate passed (build/pedestrian-final-focused-gate2.log),
and five strict finished races143764ticks passed
(build/pedestrian-render-five-finishes.json), BEFORE later ring/side-cursor fixes.
Do not cite them as acceptance for the latest source. Rebuild affected tests and
rerun both gates, Napa composed replay and the retained Highway finish comparison.
Native camera remains imported, so no native-camera or whole-race pixel parity
claim follows from composed draw-list agreement.

All current edits are owned by this rendering slice; inspect git status and stage
explicit paths only. Update shared docs with final results; commit/push the Windows
branch and publish installer1.1.33 once validation succeeds. No installer execution.
No menu or widescreen-only work, no new branch, no visual per-frame analysis.

## Updated result

Side-cursor build/replay succeeded, advancing to tick20440. The last mismatch was
FINISH material on two lane polygons, not shoulder geometry. CourseScene now
retains race level and propagates the event7/level-tagged sub+36 bit0x20 to the
road panel emitter (0045443e..00454459,0043c0b0).

build/hiway-native-finish-surface.log PASS:20710ticks identical;35windows;
1434placements/heights;9896composed nodes exact. Full Release build passes
(build/native-composed-full-build.log). Current gates sessions14044/84688,
logs native-composed-focused-gate.log and native-composed-five-finishes.log/json.
No more capture needed for this milestone. Remaining native-camera/raster/other
course-level coverage stays open. Build authorization interpretation was corrected:
the owner's explicit conversation-wide permission applies. No approval was rejected.

Final full build passes:build/native-composed-final-build.log. Expanded focused
regressions pass37/37 with no skips:build/native-composed-final-gate.log.
The first new boundary fixture had not set TerrainSlice.type=0 (default is0xff);
it is now explicit. The expanded slot state exposed Windows stack overflow
0xc00000fd in track_ring-test; ring slot storage now uses a deep-copy vector,
keeping the same128 physical slots off the stack. Both tests now pass.
All five strict completed simulations passed143764ticks before that storage-only
change; final confirmation against the rebuilt binary is running as session17120,
report build/native-composed-final-five-finishes.json. Wait for it before delivery.

## Final gate

Final strict confirmation completed successfully: five passes, zero skips, zero
known divergences, zero unexpected failures, 143764 ticks including RNG.
Report build/native-composed-final-five-finishes.json. Expanded37/37 focused
checks and full optimized build pass. Original DLL restored; no capture active.
The pending work above is historical; native-camera/unsampled rendering coverage
remains open. Commit this milestone to the Windows branch, package1.1.33 and
publish the asset-free installer without running it.
