# Parking continuation and retained road pointers

## Physical statics continuation — latest state

Published1.1.12 at98062ddd68a2e1f1912efe17cecf60543783a5e3; public installer
SHA671ffd08525dee5e8b0fdf99b039f5063eab4fe99b57ef80de7eda1cf174c2b1 verified.
Isolated1.1.11->1.1.12 preserved600 files and SDL menu smoke passes.
Initial upgrade rejected a stale precommit build stamp; rebuilt clean and reran,
then published. Logs v1112-clean-source-build, parity-v1112-upgrade-validation,
v1112-publish under build/. No unverified installer published.

Current physical-static-half fix matches immutable Hiway through11158 including
RNG; first11159 active riders5/native6 (`build/hiway-physical-static-half.json`).
Native10073 had unbuilt static table because canonical local15 differed from
physical local16. Original10090 diagnostic confirms selector128/type10/one box;
trace SHA998628e6eee06c9b8ebccf63260e42573686822e733ce3536f9688257821652f,
10090 controls validated, working DLL restored. Probe word4 mislabeled lateral
in that recording; corrected source uses neutral raw-word labels. No original
capture is active. Focused synthetic marking tests cover shifted section/local
and pass; strict Napa2400/City600 pass. Full MSVC build and SDL gate pass:104 passed,35 explicit skips,zero failures;
two unavailable Direct3D tests excluded. Logs build/physical-static-half-full-build.log
and physical-static-half-native-tests.log.
New native11159 trace identifies rider9 waking (pos361600); compare original
parked-state to diagnose wake boundaries next. No reanchor/golden edits.
Full-race/all-track acceptance remains unfinished; multiplayer held.

## Latest continuation — supersedes older WIP descriptions below

Physical force, mover-start wall reference and physical graph finish-distance
fixes match the immutable completed Hiway trace through10072 including RNG.
`build/hiway-physical-graph-finish.json`: first10073 rider13 raw_1e8 original384,
native131, exit1; full-race acceptance remains failed. Ground is force+160,
not height+24. Focused regression passes. No original capture/DLL is held.
MSVC Release build succeeds; SDL native gate104 passed,35 skips,zero failures,
two unavailable Direct3D tests excluded. Strict Napa2400/City600 pass. Logs:
`build/v1112-full-build.log`, `build/v1112-native-tests.log`,
`build/v1112-release-regressions.json`. Next trace10073 marking selector via
004049c0 byte writes. No golden change or reanchor. Multiplayer stays held.

## Current state — read before historical notes

HEAD2ff4f37b42a8352970043ad2002379cd8ac746af is pushed and released as1.1.11:
https://github.com/P0NYSLAYSTATION/road-rash/releases/tag/3do-1.1.11
Installer5330d45d6c6f37d9833328193784c11094a8dc18e7bff19c979652fe243a2679.
Public checksum verified (`build/v1111-publish.log`); isolated1.1.10->1.1.11
preserved600 files and passed SDL menu smoke
(`build/parity-v1111-upgrade-validation.json`). Release description's mistaken
ground-height label was corrected remotely without changing assets/source:
`build/v1111-release-note-correction.json`. The capture field named ground is
contact force+160; actual terrain height is+24. Do not conflate them.

Current WIP covers race.cpp/hpp, race_pedestrians.cpp, race_schedule.cpp and the
oracle diagnostic. A broad full-profile
cache attempt did not advance9749 and was removed entirely (track_ring files
matchHEAD). Actual vertical force still compared canonical current/previous
rise; reading retained physical rises resolves9749. The unchanged completed
Hiway trace now matches through9782 including RNG:
`build/hiway-physical-contact-force.json`. Strict Napa2400/City600 pass
(`build/physical-contact-force-regressions.json`). Full native gate for this
WIP still needs running. No original is currently active.

First gap9783 player half finish-distance original131780/native131812 is
downstream of the real mismatch: native crash-body motion stops (speed0 and
no longitudinal step), original continues32 Q8 with speed8213. Native trace
`build/hiway-crash-finish-delta-9783.log` shows crash entry body331548/fd131812,
then unchanged body331548/fd131812. Original body331580/fd131780. First field
is not the root cause; do not adjust finish math to hide it. Native body wall
response appears angled (speed0/f0-1200), original looks like ordinary bounce
(f0468). The completed probe confirms selector/flag/width agreement; the root
cause is previous-reference identity.0043a9d2 reads object+40, copied from+2c
at the start of that mover, while +13c remains the swept-motion previous slice.
The port conflated them. Rider.wall_previous_sample now snapshots the start
slice in bike, detached-body and pedestrian motion; car wall callbacks set it
from their prior slot. Walls uses it, while vertical force keeps +13c semantics.
`build/hiway-wall-previous-slice.json` matches through9784 including RNG.
First gap9785 rider13 finish-distance127942/native128198 is one sample.
Strict Napa2400/City600 still pass (`build/wall-previous-slice-regressions.json`).

Original exec36408 exited early3489660927 before the target probe. Its partial
capture was rejected and the exact working AWEMAN hash restored. Retry80760
completed9800 validated controls and restored the DLL:
`analysis/oracle/hiway-crash-wall-9800-retry.tsv`, SHA256
394de772b168a87b1e7481586a10d83215b2dfd3af875a6a51d5a794f966c7a0.
At9783 body entry331580/speed8213 exits331580/f0468, versus native rewind331548/
speed0. The probe's old wall-previous label read+13c; corrected source now
prints both references explicitly, helper rebuild succeeds. No second original
needed for that assembly-proven distinction.

Latest prototype addresses9785: move_bike_live checks physical ring graph nodes
for transitions, and remaining_finish_distance reads physical node/local before
graph traversal. Original rider13 moves112 but its distance drops368 when the
physical node transitions one sample before the canonical route. Replay is
running owned exec29757, `build/hiway-physical-graph-finish.log/json`. Wait for
measurement; targeted build succeeds. No physical-graph fix claim yet. If it
regresses earlier, inspect/revise the prototype rather than changing goldens.

Previous original `hiway-ground-slices-9760` completed9760 validated controls,
restored DLL, SHA2562b34c25de9a1212a2a00067238d2e7feb72b430309fde0fd69fd05486b40afee.
It confirms terrain height0 at9749; that was an unnecessary height hypothesis
because the failing field was force, and this was acknowledged to the user.
Captured current/next geometry is retained as evidence, not a parity claim.

Upstream heads rechecked: main remains20fb4c0, no new parity branch. Initial
sandbox DNS attempt failed; normal-host read succeeded. No current approval
rejection or user question. Goal API remains paused but authorized work must
continue in this turn; do not stop after a partial status. Next release1.1.12
should use outside-repo publish-v1111.py/verify-parity-v1111-upgrade.py templates.
Multiplayer remains on hold; all-track start-to-finish parity is unfinished.

Baseline be17861, branch codex/simulation-parity. Parking checkpoint1b3179d
is committed, pushed and released; the traffic wall continuation is in progress.
The ongoing user instruction is to continue autonomously until all tracks have
start-to-finish parity. Multiplayer remains on hold. The saved task API reports
the older long-running goal paused; that state cannot be resumed by update_goal.
Continue the authorized work within the active turn; do not stop after status.

## Published checkpoint

Release1.1.10 is published from clean d667918 (use git rev-parse for full SHA):
https://github.com/P0NYSLAYSTATION/road-rash/releases/tag/3do-1.1.10
SHA25669f13b90991fe99fe2f7ea0ed5ea2a1c3e30d906e80a00e0cfa01a8d841a0e48.
Public download verified (`build/v1110-publish.log`). Isolated1.1.9->1.1.10
upgrade preserved600 files and passed SDL menu smoke
(`build/parity-v1110-upgrade-validation.json`). Physical traffic checkpoint pushed.

Release1.1.9 is published from clean d9172a46eaa1d3685db75918606cfe9263f8b639:
https://github.com/P0NYSLAYSTATION/road-rash/releases/tag/3do-1.1.9
SHA2568aea62f0fd323ee3c2dbca18f4eed8dabc0488a42566cad9f82af1b91788f24a.
Public download verified (`build/v119-publish.log`). Isolated1.1.8->1.1.9
upgrade preserved600 files and passed SDL menu smoke
(`build/parity-v119-upgrade-validation.json`). Crashed-contact checkpoint pushed.

Release1.1.8 is published from clean5c36f18a36533bf93313608dd7e4d48403528237:
https://github.com/P0NYSLAYSTATION/road-rash/releases/tag/3do-1.1.8
SHA2566461bb020d6fd5ff159c4084f4c68e2e63b70f6032aceeead42ef9603915f53e.
Public download verified (`build/v118-publish.log`). Isolated1.1.7->1.1.8
upgrade preserved600 files and passed SDL menu smoke
(`build/parity-v118-upgrade-validation.json`). Police checkpoint committed/pushed.

Release1.1.7 is published from clean c2af6815ddc422fcf024a63624d382f283a79527:
https://github.com/P0NYSLAYSTATION/road-rash/releases/tag/3do-1.1.7
SHA2566ed384433e1665bc106e445157f1a2290c8f62937525471c760aed4a235af2a9.
Public download verified (`build/v117-publish.log`). Isolated1.1.6->1.1.7
upgrade preserved600 files and passed SDL menu smoke
(`build/parity-v117-upgrade-validation.json`). Traffic checkpoint is committed
and pushed. Subsequent police-state changes are in progress below.

Release1.1.6 is published from clean1b3179df89b123eb9656afb938d38fff5a6e8c2d:
https://github.com/P0NYSLAYSTATION/road-rash/releases/tag/3do-1.1.6
Installer SHA25680ebb793a9609052e0d0f5693f4401e86228f9b9dd7b0f57110b406bd14f0d7f.
Public download verified (`build/v116-publish.log`). Isolated1.1.5->1.1.6
update preserved600 files and passed installed-menu smoke using SDL
(`build/parity-v116-upgrade-validation.json`). Source build, strict regression
and installer receipts use the v116 prefix. Release notes explicitly exclude
unavailable Direct3D validation. The parking fixes are committed and pushed.

Release1.1.5 is published from clean be178614925c8fed4010aac443935b08649fc37e:
https://github.com/P0NYSLAYSTATION/road-rash/releases/tag/3do-1.1.5
Installer SHA256 a54c47de63edd4777c42454537748d01c9f7d09ed8dba8a530ae31cdf00d5103.
Public download verified (`build/v115-publish.log`). Isolated test-AppId update
1.1.4->1.1.5 preserved600 files and passed installed-menu smoke
(`build/parity-v115-upgrade-validation.json`); its preference intentionally uses
renderer=sdl. Published1.1.4 checkpoint c8f72b6 is also intact.

## Measured parking progress

Unchanged completed Hiway fixture14084 ticks, seed1, outcome3, SHA256
0fe2e9d7dd984990c528ee4b05cf80015c37b2e7a84b2c442e0b524e27d5975d.
Current replay matches through8131 including RNG with one initial anchor.
First gap8132: ring actor0x336fb08 lateral152320/native201994.
`build/hiway-standings-road-mask.json` records that remaining failure (exit1).

-6604: parked rider9 pace9619 versus9634 caused position223501 versus223502.
 Original00407610 filters opposite branches using00454040 graph flags40/80/C0
 and section+20 split identity before assigning the60/30/15 pace bands.
 Native previously included every parked rider. The graph marker now follows
 the original traversal, and each human's band filters branch membership.
 Direct capture `hiway-parked-position-6610.tsv`, SHA256
 5f04bf484f46b63e901b8cc99af90b35888b1f1653869c0bbddd3739292e297a,
 proves original pace9604 before6601 and9634 afterwards. The opposite-arm
 parked rider remains8677, while the next trailing riders receive60/30/15.
 All6610 recorded controls validate and the working DLL was restored.
-6607: original00439390 retains a discarded arm's self-linked physical
 slice even when numeric position crosses its coordinate.00437e99..00437eb7
 parks the actor when the coordinate no longer agrees with that end slice.
 Native now retains that physical sample and applies the parking test.
-6637:00450e30..00450e4d preserves tracker1's last upper endpoint after its
 self-link stops it. Native's missing-front-tracker fallback admitted the
 whole branch and woke the actor early. Physical ring location lookup and
 a copied-on-write map of closed-arm upper endpoints now bound that wake
 while tracker0 remains on the arm. Full bidirectional tracker cloning and
 arbitrary hand-over are not claimed by this forward fix.

Files changed: src/course.{cpp,hpp}, src/race.cpp, src/race_schedule.cpp,
src/track_ring.{cpp,hpp}, tests/course_test.cpp, tests/track_ring_test.cpp;
oracle/aweman_proxy.c and race.cpp additionally contain optional diagnostics.
Synthetic tests cover branch eligibility, retained physical graph locations,
discarded self-links and the retained upper endpoint. Snapshot copies share
the endpoint map; a new closure clones it only when shared.

## Validation and desktop limitation

Final checkpoint build succeeds (`build/parking-checkpoint-full-build.log`).
The fresh SDL native gate passes104 tests with35 explicit skips and zero
failures,139 registered (`build/parking-checkpoint-native-tests.log`); the two
Direct3D tests remain explicitly excluded. Strict Napa2400/City600 pass
(`build/parking-checkpoint-regressions.json`). The initial transient tooling
failure described below is superseded by this clean run, not hidden.

`build/closed-tracker-upper-full-build.log` succeeds after correcting a C++20
shared_ptr::unique compile error to use_count (first failed command recorded
in `build/closed-tracker-upper-build.log`).
`build/closed-tracker-upper-regressions.json`: Napa2400/City600 strict passes.
`RR_ASSETS=E:/RR RR_RENDERER=sdl ctest --test-dir build/release
--exclude-regex '^(directx9-main-window|integer-framebuffer)$'
passes104 tests with35 corpus/platform skips,139 registered and no failures
(`build/closed-tracker-upper-native-tests.log`). Two Direct3D tests are excluded,
not passed. A default full attempt failed five graphics/launch checks.

The published1.1.5 binary also fails when forced to RR_RENDERER=dx9
(`build/v115-dx9-display-baseline.log`); its default SDL success is not Direct3D
evidence. A separate temporary hidden-window WinAPI probe reads32-bit desktop,
1920x1080/60Hz, display format22 and CreateDevice HRESULT0x88760868
(D3DERR_DEVICELOST), `build/dx9-device-state.json`. No display mode was changed.
Do not modify the renderer or bless those failures to clear the gate. Continue
headless parity, explicitly report unavailable Direct3D checks, and retry when
the desktop/device state changes. No graphics source changed in these fixes.

## Retained-half motion evidence and next action

Original diagnostic `hiway-ped-retained-slice-6680` completed with6680 validated
controls and restored the working DLL. SHA256
0d03874705791dfc45b90cc18c0e4eb1d0b52ea157bc5829ecccd071b4be888e.
It used exact E:/RR and the measured initial extra-AI speed table.
At6670 both physical pointers remain4858724, half1, local12, encoded bend114688,
rise11. Native had read logical future common sample1040 with bend13/rise-13.
The physical half1 words survive a common-half0 rebuild. The ring now preserves
motion words separately per physical half, including copied save states.
The unchanged completed trace confirms the fix through6702. No original
capture process remains active.

`build/retained-half-motion-full-build.log` succeeds. Strict regression receipt
`build/retained-half-motion-regressions.json` passes Napa2400/City600.
The SDL suite first reported103 passes,35 skips and one tooling temporary-file
replacement PermissionError. The isolated tooling retry passed
(`build/retained-half-motion-tooling-retry.log`). Two Direct3D checks remain
explicitly excluded. This is not a clean single-run full-suite pass.

Next gap6703 is place, not a roadway-state word.00407550 tests both halves'
slice+36 masks before comparing positions; otherwise it compares finish deltas.
The route shortcut is replaced with that literal predicate using both half
mirrors' samples, also reusing refresh_place during wake. The full Hiway replay
measures exact through8131. Napa2400/City600 regressions still pass.

The next coherent traffic checkpoint needs refreshed docs/status, commit/push
and installer1.1.7 with an isolated1.1.6 upgrade check. Scripts in
C:/Users/eddj9/Documents/New project/publish-v116.py and
verify-parity-v116-upgrade.py are templates. Keep source clean throughout final
build/installer/publication checks. No commercial assets in releases.

Independent Napa moving1259 and City moving1251 still differ. Medly's old
scripted wreck is not finish evidence. All-track full-race parity is unfinished.

## Next slice in progress — traffic wall response

Original0043adc0 calls0043a610 after swept motion; the native car mover omitted
this call. A callback now reuses Race::walls with the car's type0x10 image,
before scheduling/removal.0044e0c3 and0044e3f5 also copy six kind-specific words
from475700+kind*24 into the allocation template. Native used only the common
template, losing crossing-car width and other per-kind words. Constructor
now reads that table and init_car uses it for both spawn paths.

Intermediate measurements were not sufficient for Hiway8132:
`build/hiway-traffic-wall-response.json` gives193024 and the kind-template
fix gives168448, versus original152320. No claim of a measured fix yet.
The shared wall routine applies a selector==ff widening absent in the original
result; investigate original physical selectors before changing it.

Diagnostic `hiway-car-wall-8140.tsv` SHA256
c633dd0c4de90b3a9524ff9f1702b2f6411e00805c15f5a48f9fd34957329767,
8140 validated controls, restored DLL, proves8132 car entry201994 and exit152320
with type0, flags0, shoulders-505/755. Native trace shows future physical slot
1260, same shoulders and type, but widening and originally width16384.
Enhanced diagnostic `hiway-car-wall-dimensions-8140` completed8140 validated
controls and restored the working DLL. SHA256
7084f2fd9933b0375805426c3fd4675162269aaa6359f588b00d1496699b4b61.
At8132 it proves physical selectors67/67 and halfwidth40960. The native scene
pre-filtered short buildings to selector255 before those future physical slots
were drawn, incorrectly adding widening. CourseScene now preserves raw
selector bytes; Race::sub_slice uses them beyond the32-slice draw window.
This handles the measured future-slot case, not every hidden-road draw latch.
No original capture remains active. The caller helper was rebuilt successfully
(`build/traffic-wall-dimensions-proxy-build.log`).

Synthetic oncoming allocation checks kind-specific dimensions and mass;
`build/traffic-kind-fixture-tests.log` passes. Established Napa2400/City600
pass with kind templates and selector preservation. Separate
moving Napa1259/City1251 remain unchanged (`build/traffic-wall-moving-diagnostics.json`).

Hiway then reached8144, where native eagerly cleared a car's signal reference
when returning a controller to the pool.0041f1f0 marks pool objects;00438c20
does not clear their data or other objects' pointers.0044eb89 reads the retained
signal without an active-state guard. Native now preserves returned slot data
and references; later allocation can overwrite that same slot. An intermediate
attempt to preserve references during ordinary-car allocation was removed:
0044e1d5 explicitly writes the allocation's signal, and that attempt did not
advance parity. The deferred-controller synthetic test now checks retained
signal fields and car references on teardown.

`build/hiway-signal-lifetime.json` matches through8191 including RNG, with first
gap8192 police native rider15 brain+140 original12/native0 (exit1).
`build/traffic-lifetime-checkpoint-regressions.json` passes Napa2400/City600.
Final full build and SDL suite succeeded, logs
`build/traffic-lifetime-checkpoint-full-build.log` and
`build/traffic-lifetime-checkpoint-native-tests.log`:104 passes,35 explicit skips,
zero failures; two unavailable Direct3D tests explicitly excluded. No claim of whole-race
parity. Next: finish validation/docs, commit/push, installer1.1.7 upgrade/release,
then trace8192. Do not end the turn after a partial status.

## Police personality continuation

8192 was second-appearance personality lifetime. Both original cop appearances
reference5012836 through the half's +1d8. Mutable state counters, ratings,
rating targets, easing and replan accumulator survive removal. Native now saves
those fields into PoliceSetup.config.personality before removing the cop, and
initializes state_ticks/replan_ticks from that record on spawn. No hardcoded
counter values. Initial state-counter fix passed140 then exposed188 (16/4).
Combined fix measures exact through8349 in
`build/hiway-police-replan-lifetime.json`. First gap8350 RNG original18043582,
native3799689000:2 original calls versus4 native after shared3170814756.
Original complete caller log at8350 records407ba9 and44efc6.

`build/police-lifetime-checkpoint-full-build.log` succeeds; fresh SDL suite
104 passed,35 skips,zero failures, two Direct3D tests explicitly excluded.
Strict Napa2400/City600 pass (`build/police-lifetime-checkpoint-regressions.json`).
Next checkpoint: commit/push and release1.1.8 with isolated1.1.7 upgrade check.
Read-only native trace at8350 is running in exec9891,
`build/hiway-rng-8350-native.log`, RR_FIELD_ALL1, no reseeding. No original
capture active. Continue RNG tracing after release; do not stop after status.

## Crashed-car contact continuation

8350 original407ba9 AI draw then44efc6 horn, two calls. Native consumed four
because contact_car ran the human sound branch on an already crashed bike.
00404a5f jumps directly to00404f3f in that case, skipping sounds and normal
class2/3 speed/damage decisions. Native now follows that gate and uses the car's
actual halfwidth instead of hardcoded16384 for the nudge. Corrected full replay
`build/hiway-crashed-car-sound-gate.json` matches through9335 including RNG.
First gap9336 ring.3370138.motion_12c original72/native99, kind2 car's first
backward move from338176 to338167,11-tick dt, speed242. Original physical slice
changes4877368->4876608. Test next whether traffic must read ring.retained_motion
instead of canonical route paths, as Race::accumulate already does.

`build/crashed-contact-checkpoint-full-build.log` succeeds. Initial new synthetic
test assumed no other tick RNG and failed; paired identical scene without a
car establishes the startup baseline. Corrected test checks no extra sound
draws, no class2 speed transfer and retained vertical bump. Clean SDL gate
104 passed,35 skips,zero failures, two Direct3D tests excluded
(`build/crashed-contact-checkpoint-native-retry-tests.log`). Strict Napa2400/
City600 pass (`build/crashed-contact-checkpoint-regressions.json`).

One link attempt failed LNK1104 because the previous owned replay still held
oracle-sim-test.exe (`build/crashed-car-sound-gate-build.log`). Waited for owned
exec62710 completion; retry succeeded. No approval rejection occurred.
Next: commit/push, release1.1.9 isolated1.1.8 update, then9336 motion investigation.
No original capture active. No need for another full original race yet.

## Physical traffic motion continuation

Traffic still read canonical route bend/rise while Race::accumulate read the
physical ring. A road_motion callback now resolves nearest ring retained_motion
for both endpoints of car swept segments, with canonical fallback for synthetic
fixtures without rings. This resolves9336 and matches the accepted Hiway trace
through9406 including RNG (`build/hiway-traffic-physical-motion.json`). First
gap9407 ped serial8 selector0 position314763 lateral-110745: resource+1e0 is
available original1/native0. No whole-race pass.

Build succeeds; strict Napa2400/City600 pass
(`build/traffic-physical-motion-regressions.json`); fresh SDL suite104 passed,
35 skips,zero failures, two Direct3D tests excluded
(`build/traffic-physical-motion-native-tests.log`).

An optional original ped-resource comment probe now prints graphics pointer,
RRAN header, family ID and root for target tick-20..+1. First helper build failed
because RD16 was undefined; replaced with uint16_t of existing RD32. Retry
`build/ped-resource-diagnostic-proxy-retry-build.log` succeeds. No original
capture is active yet. After checkpoint release1.1.10 (upgrade from1.1.9), run
diagnostic9408 with AI_TARGET_TICK9407, accepted controls and measured extra-AI
speeds, exact E:/RR, reversible working-DLL hooks. Compare native graphics_valid
timeline overlay against original resource header before changing residency.
Do not run full graphics/import gates while original capture is installed.
Continue without prompting the user or stopping after partial status.

## Active resource probe (latest)

Original `hiway-ped-resource-9420` completed, exact E:/RR,
9420 frames, timeout360, AI_TARGET_TICK9407, TERRAIN_TRACE1. Accepted control
file and measured extra-AI speeds are unchanged. Log
`build/hiway-ped-resource-9420-capture.log`. Wait for completion/restoration
receipt before any full graphics gate or new original capture. The original
accepted full Hiway trace remains unchanged. No reference install used.

Native read-only trace exec58329 is running with RR_RESOURCE_TRACE_SAMPLE1296,
logging `build/hiway-ped-resource-native-1296.log`; helper build log
`build/ped-resource-native-trace-build.log`. Optional CourseScene logging prints
query sample, section/local/cursor, family events/payload sizes, final four
overlaid header bytes. This is the only current source edit, diagnostic only.
No runtime behavior change accepted yet.

At9406 original ring.front340277 crosses sample1329; the native query subtracts
view_slices33 to sample1296, then CourseScene adds33 within that sample's
section. Hypothesis: crossing a section boundary changes which family timeline
is queried; subtract/add does not cancel across sections. Other hypotheses:
allocation buffer reuse or asynchronous family load timing. Compare measured
original header/family/root and native events before choosing a fix.
Ped serial8 selector0 position314763 lateral-110745 remains available through
9408 in accepted original; native first unavailable9407. No full parity claim.

No user questions pending. Continue parity until every track matches start to
finish. All authorized build/remotes/release actions remain permitted. Source
must be clean for the next final installer build/publication. Scripts outside
repo now use publish-v1110.py and verify-parity-v1110-upgrade.py as templates.

Latest resource evidence supersedes that hypothesis: native sample1296 is
section2/local166/cursor171 (bias-28), not a section-boundary mismatch. Native
overlays family171 at offset56, header28010000. Original diagnostic9420 valid
controls, restored DLL, SHA256
d9c1fd8fb3c24ccb00437b0da218a34202bf861ba01c547a6ea1f9e8e98c0644,
shows active family139/root50362024 at9405..9408. Native query used the
pedestrian's old split route, although global family slots follow the player's
committed front walker. Both load_pedestrian_looks and think_pedestrian now
query route_of(riders_[0]); test is running exec50244,
`build/hiway-ped-global-family-route.log/json`. No measured fix claim yet.

The first original ped-resource comment erroneously treated byte+1e0 loaded
flag as a pointer, so its graphics=1/header0 are invalid evidence. Family/root
fields are independent and valid. Corrected source reads animation block+4 at
object+74 and separately prints loaded; helper rebuild
`build/ped-resource-header-pointer-build.log` succeeds. No new original capture
is needed unless family/route evidence is insufficient. No original currently
active. Source WIP: diagnostic CourseScene log, corrected oracle comment,
player-route family queries, this handoff. Await full replay before acceptance.

## Current resource/lane result (latest supersession)

Changing only pedestrian->player route did not advance9407. The original
front walker has already entered the next section (position20 samples), while
the old query starts at global1296, section2/local166 and adds33 to that section
before bias-28. Section2 is only180 samples long: its event171 load is not
reached before that section's actual MSRR cursor exits. The new exact_front
CourseScene APIs query the actual global front, retain negative biased cursors
in incoming sections and stop previous sections at their terminal cursor.
RaceSetup's ring-driven callbacks select that explicit mode. Default scene
projection lookups keep their original API behavior.

`build/hiway-family-exact-front.json` advances to9516, car slice right lane
word298/native341. Physical ring lane words are now cached and TrafficSystem
uses a ring source for its lane_probe in both actual planning and verification,
including linked-neighbour widening. Synthetic traffic test gives canonical
route different lane widths and proves a stale slot sees its rebuilt physical
words after128 steps. `build/hiway-physical-traffic-lanes.json` matches through
9693 including RNG; first9694 rider13 lateral target4608/native2816.
Strict Napa2400/City600 pass (`build/family-physical-lanes-regressions.json`).
Full build and fresh SDL native gate104 passed,35 skips,zero failures, two
Direct3D checks excluded (`build/family-physical-lanes-native-tests.log`).

The new9420-tick diagnostic originally hit invalid stoul because ring.walk.*
position keys were mistaken for hex car addresses. tests/field_replay.hpp now
excludes that known diagnostic prefix from car address enumeration. Afterward
all standard compared fields match9420 ticks with binary exit0, but strict
verify still rejects121 additional un-compared terrain diagnostic fields:
`build/family-physical-lanes-original-diagnostic.json` is NOT strict acceptance.
Do not weaken that check, drop fields from the accepted original or call it a
strict pass. The unchanged full original is the primary acceptance fixture.

Latest WIP adds physical cached bend reads to Race::curvature (00449b40's
slice-link lookahead), testing the9694 target difference. Owned exec29831:
`build/hiway-physical-ai-curvature.log/json`, targeted build succeeds. Wait for
measurement before claiming this fix. No original capture is active. After
coherent validation/docs, commit/push and release1.1.11 (upgrade from1.1.10).
Source currently also includes optional resource timeline and phase logging,
corrected original header probe (+74 instead of loaded byte+1e0), exact-front
family APIs, physical lane cache/ring source, synthetic lane test and parser
prefix fix. Preserve all; no other agents are editing. Continue autonomously.

Physical AI curvature is now measured: `build/hiway-physical-ai-curvature.json`
matches through9748 including RNG. First gap9749 rider13 ground64/native71.
Actual00449b40 uses physical bend words; the canonical fallback caused the
seven-world-unit9694 target mismatch. The same ring cache now supplies these
lookahead bends. Strict Napa2400/City600 pass
(`build/family-lanes-curvature-regressions.json`). Final full build and SDL gate
succeed (`build/family-lanes-curvature-native-tests.log`):104 passes,35 skips,
zero failures; two Direct3D checks explicitly excluded. Ready for checkpoint
commit/push and1.1.11 installer/release (isolated1.1.10 upgrade).
Next independent read-only task: trace9749 rider13 ground and compare physical
ring terrain/side heights against the canonical section/local source. No
original capture active. The latest original prefix's bad header probe is
explicitly excluded as evidence; corrected helper is built for future probes.
