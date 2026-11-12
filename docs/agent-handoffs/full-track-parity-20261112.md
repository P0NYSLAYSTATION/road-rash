# Full-track parity continuation — November 12, 2026

Baseline6a42fbd on codex/simulation-parity. User requests every track from
start to finish, then specifically asks to verify input masks and road centre.
E:/road-rash multiplayer edits are untouched. Last published installer remains
3do-1.1.2 from15b46a1. The changes below are local, uncommitted work.

## Current changes and measurements

- Controllers now dispatch between rider/pedestrian/car nodes by position,
  rather than consuming all their random draws in a prepass. Newly spawned
  cars behind the controller cursor execute in the same scheduler pass.
  Original City751 randlog has two six-draw car spawns, an AI draw at409377,
  then another two spawns. This explains the former differing car vehicles
  and acceleration with an equal final RNG. City first gap751 ->825.
- Mixed scan-list sorting retains prior order at equal positions, rather
  than imposing creation-type order on car/rider ties. Refresh that history
  at both original sort boundaries. Napa first gap1072 ->1259; City825 ->881.
  This history is copied by RaceState but still needs inclusion in state_hash
  and a focused mixed-node ordering regression before acceptance/commit.
- Pedestrian spawn and think read the player's rider half, not its bike:
  original4409f7 and440deb dereference player+2fc then+1c. Correct both paths;
  City881 ->1251 (pedestrian state original3/native0). Current first Napa
  gap1259 is rider4 position/lateral/speed/contact response. No whole-race
  acceptance claim. Reports build/mixed-scan-parity.json,
  build/city-pedestrian-half.json and build/napa-1259-all.log.
- Full-race validation now streams one frame at a time. All21 pre-existing
  tooling tests passed before adding the bounded-memory regression.
- Indexed C++ replay validates the whole trace, retains a bounded decoded
  frame cache and pins startup/green anchors. Eager parsing remains available
  for component fixtures. New1000-frame lifetime/backward-access/late-malformed
  tests pass (golden-test reports0 failures). Full Release build finished
  before interruption, build/full-race-tooling-build.log. Focused current
  golden/tooling/traffic/pedestrian/scheduler gate is in
  build/continued-parity-focused-tests.log. Full integration gate is pending.

## New Hiway capture and driver limits

hiway-human-follow-6000.tsv is complete,6000 ticks, human branch, seed1,
level1, DRIVE2, with a recorded .controls file. SHA256
7870c42985b7e8b00be87e67f99d2c055a85193d7300cbde200f1efe1b6541c8.
Receipt says phase restored. Its first native gap is already tick1:
ped.32f79c4.28 original80637/native81920 (build/hiway-follow-parity.json).
This is a different setup/spawn slice, not evidence of full track parity.
The driver's poor driving must be corrected before relying on finish coverage.

Inputs are raw control-poll return masks, not simulated key presses. Current
DRIVE2 always accelerates, steering toward slice+cc only beyond a60-unit
deadband. It does not predict heading/curvature or adapt speed. Original
44fa80 keyboard conversion ORs masks from476c00; original43490d/434921
consume4 and200 as left/right, and43489b consumes1 as acceleration.
The masks themselves are confirmed from the working E:/RR/ROADRASH.EXE.

Important analog dependency: original40ad35/40ad66 checks476c98 (joystick
mode). In that mode it reads4b7e7c for steering amplitude. Keyboard conversion
44fa50 normally updates that axis through44fa00; raw-return injection bypasses
those side effects. Thus masks alone may be insufficient. Do not declare
joystick mode the actual cause until the prepared runtime probe is run.
First-frame input.joystick and input.steer_axis fields were added to the
helper source but that helper has NOT been built or run.

Road-coordinate evidence: original44a032–44a053 takes bike+28, subtracts
(slice+cc)<<8, then divides by256. The current bot's bike>>8 minus slice+cc
therefore has matching units. cc is the section/roadway reference origin
(4542a2–4542a5), not necessarily the pavement midpoint. Actual left/right
edges e0/e4 are derived independently from lane widths at45439c/4543be.
For unequal widths, midpoint(e0,e4) differs from cc. The bot currently reads
the retained physics slice at bike+140; live slice+2c, roadway selection,
fraction/forward-link interpolation and a lookahead target still need runtime
verification. Do not guess an extra256 scaling correction: that would be wrong.
Read-only disassembly evidence is in build/player-input-original.asm,
build/input-poll-original.asm, build/slice-centre-build.asm and
build/ai-road-centre.asm. No new oracle reference copy is used.

## Blocking condition and next actions

### Superseding runtime verification, November12

The helper compiled successfully in build/input-mode-helper-retry.log. Native
hiway-input-mode-4 measured input.joystick=1 and input.steer_axis=0. Masks4
and200 select the correct directions, but with joystick mode enabled the
original40ad78 reads the axis for right-steer amplitude. The raw capture
injection had bypassed44fa80's calls to44fa50, leaving right steering at0.

Fixed injected keyboard input by invoking the original44fa50 for accelerator,
left and right actions before each simulated step. This preserves the original
joystick setting and reproduces keyboard side effects instead of changing
the driving physics. Right now supplies axis32; left supplies0, which the
original left branch subtracts32 from. Playback of recorded masks uses the
same conversion.

Driver target uses the active roadway slice (+2c), with retained physics
slice+140 as fallback, and pavement midpoint(e0,e4) instead of reference
origin+cc. Centre and bike lateral share units after the bike's Q8 conversion;
there is no missing factor256. Comment-only per-tick driver diagnostics record
road edges, reference origin, bend, pointers and actual axis/masks.

hiway-keyboard-midpoint-3000 is a complete human3000-tick diagnostic, with
recorded controls and restored DLL. All3000 ticks lie inside pavement edges;
2589 have nonzero curvature. Right460 ticks all have axis32, left518 all have
axis0. In this run midpoint==origin and active==retained on every tick, so
the demonstrated fault is the missing axis update, not a corner-centre scale
error. Splits/unequal widths still require runtime coverage. Evidence:
build/hiway-driver-verification.json. No full-race parity claim follows from
this driving check.

Full Release CTest on the current local tree passes141 registered tests:
105 passed,36 explicit corpus/platform skips, zero failures
(build/full-track-integration-tests-console.log). Indexed golden-frame and
streaming validator lifetime/memory regressions pass. The local simulation
changes above remain separate unfinished work; do not stage them with the
tooling/input correction. The old quota failure described below is historical
and is no longer a blocker.

The attempted `oracle/build-windows.ps1 -Kind capture` for the input-mode
probe was NOT executed: automatic approval review failed on usage quota,
reset quoted November12,03:18. Review explicitly says this is a review failure,
not an unsafe-action decision, and prohibits bypassing it. Do not retry via a
different execution channel or use another install. Existing read-only tools
and already-built focused tests can still run. No remote/installer mutation
has been attempted since that rejection. User has already authorized builds,
commits/pushes and releases; more user permission is not the missing condition.

When approval review works again: build the prepared helper, capture a short
input-mode/road-coordinate diagnostic only from E:/RR, verify restoration,
and correct the driver based on measured mode/coordinates. Complete focused
regressions, state-hash coverage and full native/oracle gates for the local
simulation changes before committing/pushing/releasing. Then resolve the
retained first gaps and expand all five tracks and difficulty variants through
actual finish outcomes. Partial capture equality is not start-to-finish parity.
