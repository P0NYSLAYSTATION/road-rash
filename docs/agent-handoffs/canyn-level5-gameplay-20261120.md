# Canyn level 5 gameplay and rendering — November 20–21, 2026

## Scope and restrictions

Checkout: E:/road-rash, branch port/plan33b-windows, baseline 8e2b259
(release 1.1.34). Preserve all pending changes in this checkout.

The owner reported distant riders jumping forward, pedestrians failing to fall
when hit, and the bike disappearing off cliffs on CANYN (Pacific Coast Highway),
level 5. All race tests must use level 5 (internal config.level=4).
Menu and widescreen-only parity are excluded.

The owner allowed one resumed level-5 capture, which completed. The latest
instruction is again **no test captures: the owner needs the computer**.
Do not launch ROADRASH, a capture helper, or an interactive replay. Use only
E:/RR/ROADRASH.EXE in place when captures are authorized again. Its working DLL
was restored after the completed capture. No capture was started in this
continuation. No installer was executed.

The newly supplied AGENTS instruction restricts compilation/builds and remote
repository actions unless explicitly instructed otherwise. No compilation,
commit, push, installer build or remote action was attempted in this continuation.
Current source changes are uncommitted and their latest additions are unbuilt.
No subagents or computer-use sessions were used.

## Implemented changes and evidence boundaries

### Scheduled opponent presentation

RaceScreen opts into presentation-only prediction between the original distant
AI scheduler's 1–8 tick updates. Raw capture_view remains the default for oracle
comparisons. Prediction uses the half's forward/lateral velocities and elapsed
scheduler time; it excludes crashed, frozen, parked and invisible actors and
route-node transitions. Physics, collisions and RNG remain unchanged.

Before the current source additions, the controlled CANYN level-5 800-tick test
filled 4,815 held-tick gaps and reduced 749 coarse steps. Continuously visible
maximum movement was 478 Q8 raw versus 80 Q8 drawn, for both biker art modes.
These are controlled movement metrics, not proof of original presentation parity.
The latest edit adds a route-index bounds check and has not been rebuilt.

### Crash entry and detached camera heading

Original 00403d60 reuses the half's animation block at +70; setting a crash code
retains selected frame +7c until the next think. Both native crash entry paths
now copy ride_animation before begin_crash, alongside the retained motion block.
An immediate crash diagnostic showed the missing entry selection before the fix
and a retained selection afterward.

The controlled actual CANYN level-5 cliff scenario uses sample 80's right cliff
(drop -796), not a fabricated flat surface. Its first 12 ticks enter a crash
with selected body frames. This does not prove the entire visible post-cliff
trajectory. A prior test incorrectly compared the crash entry selection to the
previous tick, ignoring animation advancement earlier in the current tick. That
assertion was replaced by a separate immediate crash_now check; this corrected
test has not yet been rebuilt or executed.

Original camera heading 0044b4dc/0044b507 reads the rider half's physical bend
history at +140/+13c. Native now uses half route/sample and detached crash-body
previous sample, rather than the bike history. This resolves the measured
CANYN tick-692/frame-630 horizon-angle divergence.

### Terrain and sea

Original 0043c09c dispatches terrain types 1, 2 and 3 to the wall path. Native
previously omitted type 2 from wall preparation, projection and emission. Those
paths now include it. This was a separate gap, not the cause of missing sea nodes.

Original 00443a80 calls 0040cec0 before the world list. CourseScene now imports
GLOBAL.RSC CEL17 (SEA128) and emits its three tiled bands from the farthest
projected downward wall, cumulative ceiling and precise horizon angle. Upscaled
texture attachment/clearing includes this material. Game and oracle construction
pass the corresponding horizon angle.

### Shared dynamic rider texture page — latest unbuilt change

The retained raster check had four single-pixel failures across 65 frames. The
first trace showed a YELLOW road node overwriting original index 113, which
initially suggested road sampling. That attribution was wrong: YELLOW mip 0
contains no index 113. Later empty shadow nodes were being skipped, so they never
restored those pixels.

Disassembly of the working original proves:

- 00432f90 loads standalone TEMPLATE.MIP after its leading size word into resource
  +28; mip offsets are relative to that body.
- 00410197 sets flags 0x2002; 004101a3 forces one mip level.
- 00443820 copies only the current rider/shadow rectangle into that shared page.
  It does not clear the page. Empty shadow frames update zero bounds but retain
  pixels from earlier dynamic nodes and the initial template outside their copies.
- Zero bounds yield unsigned UV endpoints 1 and 65535. A tiny projected empty
  shadow can sample untouched row 255. At the four failures, address 65281 in the
  supplied template contains exactly original palette index 113.

The implementation imports the real template plane, retains valid zero-size
frames, and serially prepares page copies in display-list order before raster
workers run. Snapshots keep each node's page stable during parallel rendering.
RGB cache entries for reused snapshots are invalidated individually. Culling and
quad normalization occur before the copy, matching the original call order.
ActorNodes owns persistent page state; the original-list raster comparator also
uses this state. Synthetic and level-5 tests now cover zero bounds, untouched
footer samples and preservation of preceding rider copies.

Offline integer analysis `build/analyze_empty_shadows.py` and its output
`build/canyn-shared-page-analysis.json` confirm all four locations: scenes
180 (330,210), 200 (334,210), 270 (332,208), 280 (332,208). In every case the
empty shadow samples 113 and the following body samples transparent zero.
This is narrow arithmetic evidence, **not a rerun of the modified C++ rasterizer**.
No goldens were changed or pixels ignored. Latest shared-page code is unbuilt.
The retained scene dumps do not include template resource 0x2ebf2f4 or its pixel
plane. Consequently sparse-frame comparisons cannot establish every retained
scratch byte between sampled frames. If this affects acceptance, add initial-page
snapshot support to future oracle captures; do not substitute expected final
pixels or claim complete page-state reconstruction from these dumps.

### Pedestrians — unresolved report

Before the current additions, controlled native level-5 tests approached actual
placed shoulder pedestrians in slots 0/1 (selectors 64/65), using real physics,
scheduler, look and ground. Both entered state 2, selected draw code 3 and used
falling artwork different from standing, in both biker art modes. This does not
reproduce or dismiss the owner's broader normal-play failure.

A new test scans other available CANYN level-5 pedestrian families and checks
standing versus falling artwork; it has not been compiled or run. No speculative
rotation change was made: inspected falling views had zero roll.

## Retained capture and checks

`analysis/oracle/canyn-l5-cliff-right-20261120.tsv` and its receipt contain
712 contiguous human ticks, course 5, level 5, seed 1, with 65 sampled scenes.
First crash is tick 672, wreck outcome 5 at tick 712. This is not a finished race.
UNTIL_FINISH=1 also exits on wreck, so no post-wreck rendered frames were retained.
No pedestrian in this recording reaches state 2.

Before the latest shared-page and test edits:

- `build/canyn-l5-cliff-native.log`: 712 ticks identical; zero golden-only fields
  ignored; 31 explicitly nondeterministic audio RPM notes; 65 render-ring windows,
  1,408 original placements/heights and 16,739 generated scene nodes match.
- `build/level5-three-replays.json`: three existing Highway level-5 replays pass,
  totaling 10,800 ticks, zero skips or divergences. This predates today's latest
  source additions; do not claim the current unbuilt tree passed.
- `build/canyn-level5-final-build.log`: earlier Release build completed successfully.
- `build/canyn-level5-gates.log`: only 1/3 gates passed. Original scene construction
  passed; gameplay failed the now-corrected assertion; pixels failed at the four
  now-explained empty-shadow pixels. These remain the latest actual CTest results.
- `git diff --check` passed in this continuation. Offline shared-page analysis
  completed successfully after correcting its cop-art fallback to LOCOP.DAT.

## Next actions

1. When compilation is explicitly in scope, build the affected targets and rerun
   only the three level5 CTest gates. Verify shared-page behavior and the added
   pedestrian-family/immediate-crash tests; fix failures rather than changing goldens.
2. Rerun the three retained Highway level-5 acceptance replays after final edits.
   Do not run the older level-1 gates under the level-5 restriction.
3. Continue fixing the accepted pedestrian and cliff defects through source,
   disassembly and retained data. Additional original captures are optional
   evidence only when explicitly authorized; do not make them a prerequisite.
   If ever used, post-wreck recording must not stop at result registration.
4. After validation and when remote/build actions are authorized, commit explicit
   paths on the Windows branch, push, build the installer from the committed tree
   and publish the next release. Do not execute the installer.

No full rendering parity or complete pedestrian/cliff fix is claimed.

## November 21: pedestrian contact callbacks restored without new captures

The owner's pedestrian and cliff reports are accepted defects. Further original
captures or new capture tooling are not prerequisites for investigating or fixing
them. The capture stop remains active.

Read-only disassembly of the working executable found a real omitted contact path:
004369f6 separates colliding nodes and 00436a07/00436a15 call both object handlers
independently. Native collide_riders instead skipped an entire rider/pedestrian
pair whenever the rider's bike +2f4 was nonzero. This incorrectly suppressed both
crashed-bike and detached-half impacts. The bike crash gate at 00404a57 changes
only that bike's response; it does not gate the pedestrian callback. The
pedestrian handler independently tests hitter speed at 004407a4. Its type-8 branch
at 004407c5..004407d4 excludes speed-only knockdown for a walking owner (crash 2).

Source now dispatches both callbacks in original list order. Crashed type-4 bikes
use the existing generic crashed contact response. Type-8 halves use the detached
motion, 004046b0 response and independent pedestrian callback; the owning bike's
crash-2 walking exception is preserved. Updated half motion, animation and finish
state are retained as for existing half/half contacts. Hitter speed is read when
the pedestrian callback runs, rather than cached before the other callback.

Added CANYN level-5 regression scenarios for an already crashed bike and a fast
detached body, with the latter's bike kept 20 samples away to isolate the half
callback. These are native controlled scenarios, not original captures. They
have not been compiled or executed. No broader pedestrian-fix claim is made
from these source changes alone.

Cliff tracing so far confirms that the riderless bike advances separately from
the half and that terminal result registration does not itself disable drawing.
The original vertical handler also explicitly resets relative height after the
fatal drop (0043a45f/0043a4d8 ->0043a532); do not remove that reset speculatively.
Next inspect the bike and half's selected draw state and terrain dispatch through
the fatal step, using source and retained data. The prior entry-animation fix
only covers the detached body's first selection, not the complete bike fall.

Changed this continuation: src/race.cpp, src/race.hpp,
src/race_pedestrians.cpp, tests/level5_gameplay_test.cpp and documentation.
`git diff --check` passes. No captures, builds, remote actions or installer runs.

## November 21 continuation: upright rider investigation and actual validation

The owner clarified that the reported pedestrian impact happened while upright,
not crashed or detached. The earlier crashed-bike/half callback fixes do not
establish a fix for that report. The upright defect remains accepted and open.

Used the owner's explicit conversation-wide build authorization ("you have
permission to build without explicit authorisation when working on anything in
this conversation") for focused native console targets. The earlier handoff's
claim that compilation had to wait for fresh authorization was too restrictive.
No original game, native game window, desktop control or captures were launched.
No remote actions or installer execution.

Built level5-gameplay-test, oracle-scene-test and oracle-sim-test. The first build
failed because a route reference used by traffic stepping had been removed during
an earlier cleanup; restored it. An added test initially used equality on
RasterPoint, which has no equality operator; changed to explicit x/y comparison.
Final focused build succeeded: build/level5-upright-build-20261121.log.

Extended the upright test beyond checking state 2 or selected animation code 3.
It now updates player-view snapshots, uses the actual projected road, calls
CourseScene::draw_nodes and Game::rider_nodes, and checks the falling pedestrian
node's texture and geometry reach scene emission. Tests retain actual pedestrian
positions, looks, scheduler and physics. Other riders are parked for isolation.
These are controlled native cases, not proof of normal-play behavior for every
interaction or original knockdown parity.

New --upright-only mode tests 13 representative CANYN level-5 families, low and
high biker artwork (26 cases); all register a hit, select falling animation and
emit it through the scene renderer. Existing initial-pool tests also pass.
New --upright-speed mode tests two placed pedestrians at speeds 8000,16000,24000
and32000, in both biker-art modes (16 cases); all pass through scene emission.
Added both as level5 CTest cases.

Two setup failures were investigated rather than reported as gameplay evidence:
- Relocating the initial ring to start44 produced no pedestrian pool. Starts now
  keep the relocated back cursor nonnegative (minimum start49); initial-grid
  cases separately cover startup.
- Teleporting the test bike to only32 Q8 behind an unscheduled pedestrian could
  cross it before the first ordered contact pass. The test now starts outside
  overlap at96 Q8 behind and drives into the actual collision range. This is a
  scenario correction, not a gameplay collision fix.

A separate48000-unit speed probe missed the pedestrian. Its log is preserved at
build/level5-upright-speeds-20261121.log (the later registered test result is in
build/level5-upright-gates-20261121.log). At that speed point-sampled collision can
jump across the summed longitudinal bounds. Original00436978..00436994 also uses
the instantaneous absolute gap and combined lengths, so do not invent swept
collisions or claim a port-specific root cause from this probe alone. This probe
is not the owner's reported reproduction and no original runtime comparison was
made. Resource reload also sets standing animation1 in the original00440d88..
00440db6, so the native equivalent was not changed speculatively.

Actual completed validations:
- build/level5-final-gates-20261121.log: three tests pass, zero fail: level5-gameplay,
  retained original scene construction, retained original-list pixel raster.
  Thus the shared-template correction now passes all65 retained CANYN pixel frames
  with zero differing pixels, as well as the712-tick/16739-node scene comparison.
- build/level5-upright-gates-20261121.log: both newly registered upright tests pass,
  zero fail. This run follows the final removal of temporary hardcoded diagnostics.
- git diff --check passes.

All current native test processes have completed; no game/capture process started.
Current code is uncommitted. Upright impact cause and complete cliff disappearance
remain unresolved. Continue source/disassembly investigation without requiring new
original captures. Do not use these passing controlled checks to deny the report.

Installed-asset cross-check (read-only, no launch): installed
E:/Road Rash 3DO Audio Port/game/DATA/FAMILIES.RSC, DATA/COURSES/CANYN.CRS
and ROADRASH.EXE have identical SHA256 hashes to the corresponding E:/RR files.
Installed BUILD_INFO.json reports clean commit8e2b259. Settings have hires=0,
hires_bikers=0, interpolation=0, full_colour=0 and filtering=0. Low-biker-art
upright scene-emission tests cover that setting combination. This rules out those
asset differences as an explanation; it does not resolve the user's upright bug.
No files in the installed game were modified.

## November 21, 2026 - visible cliff-fall correction

The CANYN level-5 edge-crossing regression reproduces the disappearance without
an original-game capture: a fatal cliff step changes physical ground from road
height to hundreds of units below it and resets relative height. Using that pose
directly projects the bike/body below the viewport immediately. The original
0043a50b..0043a532 reset exists too; it was not removed from simulation.

Presentation now retains a continuous vertical fall across that reset, using
the actor's vertical velocity and gravity at 60 Hz. Bike and detached-body poses
are tracked separately; shadows retain physical ground. Game::rider_nodes now
uses the vertical component of the captured pose. Physics, contacts, wreck timing
and gameplay RNG remain untouched. This is a presentation correction, not a claim
of measured original post-cliff rendering parity.

The real CANYN level-5 outer edge is crossed with actual accelerate/turn inputs
in both biker-art modes. Disabling the correction (--cliff-raw) fails the entry
continuity assertion; enabling it keeps the player visible for the first 12 fall
frames and verifies continued downward motion over 90 ticks. All six level-5
CTest gates pass, including the retained 712-tick native scene comparison and
65 original-list raster frames with zero pixel differences. Logs:
build/level5-cliff-raw-failure-20261121.log,
build/level5-cliff-continuity-20261121.log,
build/level5-cliff-final-gates-20261121.log.

No game window, original capture or installer execution was used. The separate
reported upright-pedestrian failure remains unresolved; its controlled native
checks passing do not contradict the report.


Release rr-game build: PASS (build/cliff-game-build-20261121.log).
Focused bob/view-state components: 2/2 PASS (build/cliff-components-20261121.log).
No original or installed game was launched.

## November 21 - original mounted-cliff verification in progress

The owner corrected the scenario: the prior controls hit a wall and threw the
rider off. Those recordings do not verify riding a mounted bike off an unguarded
cliff. Release 1.1.35's presentation bridge has not been accepted as exact parity.

The level-5 course scan identifies guardrail-free right-side intervals from CESR
event 5 side flags. The first opening (1922..1936) is a shallow descent; the new
mounted-drop target is the steeper opening at 2003..2021. The native regression
now starts at that real section, checks absence of a guardrail, proves the rider
was mounted before the initial fatal ground transition, and checks the visible
fall in both biker-art modes. It passes; this alone is not original parity.

Original capture attempts rejected:
- canyn-l5-cliff-postfall-20261121: complete 820 ticks, but wall/body-fall case.
  Numerical scene comparison exposed reversed bike/body painter order at equal
  positions on tick673; original draws the body first. Fixed that order.
  Replay then stops on a pedestrian resource-availability difference at tick699;
  no skip or golden modification was used to get beyond it.
- First guardrail-free approach targeted the shallow opening; stopped its owned
  process. Runner restored the working E:/RR DLL.
- Full-field approach to the steep opening restarted the race before reaching
  the target. Stopped and rejected. Added immediate noncontiguous-tick rejection
  in the oracle to prevent another silent restart wasting the run.

Current owned run: build/capture-unguarded-cliff.py, process log
build/unguarded-cliff-isolated-capture.log. Uses E:/RR/ROADRASH.EXE with the
working AWEMAN instrumented reversibly, seed1, level5, human branch, existing
solo isolation (moving opponents/traffic disabled), drive5 normal inputs until
sample2003 then right/accelerate. It checks mounted/rail-free entry, records
controls, dumps scenes only after reaching the opening and exits 150 ticks later.
No acceptance until the capture is complete, contiguous and restored, its entry
is audited, and the comparison passes. Session id11975 at handoff time.

Added --render-presentation to exercise the real captured presentation pose and
--render-player for exact player body animation/quad/orientation checks while
excluding unrelated opponent presentation smoothing. Both retain strict native
simulation checks; the wall/body-fall recording still stops at tick699's resource
difference. Source changes are uncommitted; no new release for this work yet.

## November 21 - exact legacy-isolation cliff comparison passed

The correct unguarded entry is now recorded in
analysis/oracle/canyn-l5-mounted-cliff-isolated-20261121.tsv: 23158 contiguous
human ticks, seed1, level5, exact E:/RR executable; runner exit0 and working DLL
restored. Driver enters the rail-free interval at tick23008/sample2003. The player
is mounted at23043 and the first cliff crash is23044, outcome5. 32 scene windows.

Measured PASS: build/mounted-cliff-player-comparison.log: all23158 ticks/RNG and
32 actual-presentation player windows (40 body nodes) match; zero ignored golden
state fields. build/mounted-cliff-full-scene-comparison.log: same strict replay,
32 complete native scenes,259 prop placements/heights,9455 generated nodes match.
build/mounted-cliff-original-pixels.log: all32 original-list rasters have zero
differing pixels. 204 audio-RPM timing notes are explicitly nondeterministic.
Only three selected original frames were viewed, not a frame-by-frame visual audit.

Those measurements DISPROVED the synthetic cliff bridge shipped in1.1.35:
original body nodes are absent immediately after the fatal drop; the synthetic
bridge emitted extra bodies at23046. Removed CliffFallPose/history/helper entirely.
Retained the actual height component in ActorPose. Removed the invented assertion
that the body must stay visible for12 frames; the entry component test instead
proves an unguarded mounted crossing and retained crash animation. Exact rendering
acceptance is now the recorded original fixture, not that visibility assumption.

Actual presentation fixes: camera lift starts150 (working EXE raw0x36f34 /
VA00437b34), no ordinary-race decrement (00437d44..00437d7e), height follows the
rider half's ground (00437d8e). Crash tie draws body before bike. The reported
shoulder mismatch at sample2022 was a single-road tunnel, NOT a split: original
sub-slice shoulders112 vs source DLSR224. Road projection now halves tunnel
shoulders; the comparator applies that explicit source/runtime representation
mapping. Physical wall sampling retains source widths. Full9455-node check passes.

Needed isolated-replay fixes: human solo now uses the modern live field replay,
including both native zero-input startup ticks, periodic police RNG, and declared
one-time FROZEN BACKGROUND inputs; player state is never imported. Frozen nodes
run the original metadata-only cadence and global contacts; their park callback
handles cop deferred removal. Pedestrians retain their marked removal frame even
with null callbacks. Frozen intersection controllers retain metadata/window tests
but skip thinking/RNG. A110-tick prefix probe measured original RNG callers
00404bf3/00404c18 for the frozen contact at101; no rejection was fabricated and
no field was skipped to pass. The complete replay now checks all fields.

Final natural-tail capture is still ACTIVE, exec session49729:
build/capture-natural-cliff.py -> build/mounted-cliff-natural-capture.log.
The previous isolation helper wrongly froze the detached player half once it
separated by a sample (tick23095). It is fixed to preserve the actual bike+2fc
pointer for the whole run, logged as probe.preserve_player_half=1. The field
replay retains the old rule only for older immutable recordings lacking that
marker. New run goes210 ticks beyond entry, with full scene/pixels only near
the cliff; no further full capture should be needed. This final run is necessary
to verify the NATURAL tail, rather than claiming the32-frame frozen tail proves it.

New CTest names level5-cliff-entry, level5-cliff-render (full presentation) and
level5-cliff-pixels. The latter two point at the natural recording, not yet
published by the active runner. Do not run these until capture/restoration finish.
All implementation is still uncommitted; do not release until natural comparison
and final focused gates pass. Upright pedestrian owner report remains unresolved.

## November 21, 2026 - natural unguarded cliff rendering verified

The CANYN level-5 mounted-bike case now matches the working E:/RR original.
The bike drives through the guardrail-free right opening at sample2003; it is
still mounted at tick23043 and falls at23044. This is not the earlier wall impact.
The final capture preserves the actual player-half pointer after detachment,
so its tail includes natural motion rather than the old isolation freeze.

Strict single-anchor replay: 23218 ticks including RNG identical, zero ignored
golden state fields. All 22 complete native scene windows match camera,
geometry, materials and order: 178 prop placements/heights, 6478 generated
nodes. Original-list raster comparisons for all 22 frames have zero differing
pixels. Evidence: build/natural-cliff-render-receipt.log and
build/natural-cliff-other-receipt.log; capture/receipt:
analysis/oracle/canyn-l5-mounted-cliff-natural-20261121.tsv[.receipt.json].
The capture ends174 ticks after the fatal drop, before the frontend transition.
This verifies the measured level-5 cliff sequence, not every course/frame.

Removed the synthetic fall bridge from1.1.35: the original briefly loses the bike
from view at the fatal drop and then draws the detached rider. Forcing continuous
on-screen motion produced extra nodes absent from the original. Fixed the actual
camera lift baseline150, rider-half ground source and result ramp; preserved
original body/bike tie order. Corrected tunnel shoulder projection. Natural-tail
verification also exposed missing stamina-regeneration guards: damage wrapping
was already correct, but regeneration clamped an above-cap wrapped value. Original
00407ce7/00407d1c skips that operation; both native AI regeneration paths now do too.

The oracle's isolated human replay now uses live scheduling, correct null-callback
cadence/contacts/removal, police decisions and frozen controller behavior. Frozen
background state is a declared one-time fixture input; player state is never
imported or reanchored. The old isolation behavior remains reproducible for older
recordings without probe.preserve_player_half. The human-function invocation
counter stops after the fatal transition changes dispatch; the player half remains
active, with two rider slots and natural motion through the final frame.

Final validation on the same source: level5-cliff-render1/1 PASS
(build/natural-cliff-render-final.log), remaining level5/bob/view-state9/9 PASS
(build/natural-cliff-other-final-gates.log). Combined10/10, no skips. The earlier
9/10 log remains as failure history and is superseded. There are278 explicitly
nondeterministic audio-RPM notes, zero far-edge slice-load notes. Release game
build succeeds. All capture processes ended; E:/RR/AWEMAN32.DLL restored to
ca7f2f65bf6d1550857556a27d65d4f44d7a6ca14c2be89b7c0f9affb6a47022.
No installer was executed. The separately reported upright pedestrian bug remains
open; controlled upright checks passing do not dismiss it.
