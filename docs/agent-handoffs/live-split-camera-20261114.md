# Live split rendering and camera origins — 2026-11-14

Baseline: `port/plan33b-windows`, `ff35ddb` in E:/road-rash.
Only the working E:/RR/ROADRASH.EXE was read for original disassembly.

## Cause and correction

The previous split regression used immutable all-left/all-right routes. It did
not exercise the actual ring's departing collapse and approaching merge window.
That coverage did not establish that live split rendering was correct.

The race screen now overlays its visible route window from current physical
TrackRing cells: graph node/local, physical half, offsets, separation/slope,
shoulders, and real signed split countdown. Render reads validate the slot's
longitudinal position and normalize common roads to half0; the separate
retained physics API still exposes discarded/stale halves deliberately.
Riders, cars and pedestrians use the same live projection metadata.
The already-joined live hill profiles are not joined a second time. The inward
band limits are recovered from their collapsed endpoints using the ring's
separation. Live shoulder-height overlays reach projection. Marked/intersection
exterior dispatch no longer replaces the dual interior hill/separator path.
Opposite-road visibility now uses the existing recovered 00444553..004445d0
rule, including physical camera half0's terrain type, with original depth limits.

Working-original 00437c30..00437cf8 blends branch/common road centres over16
whole samples before lateral chase following. Departures use -1-local;
merges use section length-local. The native chase camera now implements this
signed, truncating blend using actual physical ring edges. There is no generic
camera smoothing added.

00443a1e..00443a42 updates the selected rendering half only while the player
is on a dual slice. The committed branch remains selected when the player
reaches common half0 but the one-sample-behind camera is still dual. Native
camera origin and visible route selection now preserve that branch.

ViewState carries the physical origin used by its relative camera X. When
longitudinal interpolation draws the preceding slice, Game rebases the latest
absolute camera into that drawn slice. Using the newest slice's relative X
against the preceding slice's offset could expose a one-frame branch snap.
The new field is presentation-only; snapshot wire formats are unchanged.
Ring sample separation fills already-serialized fields, without changing RNG,
walker advancement or simulation terrain.

## Evidence and checks

MSVC Release rr-game and affected test executables build successfully.
Final focused CTest:16/16 pass, zero failures/skips (50.45 seconds).
Final focused CTest command (RR_ASSETS=E:/RR, SDL_VIDEODRIVER=dummy):

    ctest --test-dir build/release -R '^(race-live-splits|race-screen|race-rider-reactions|race-start-camera|race-crest-canvas|road-projection|view-state|track_ring|snapshot|snapshot-native|snapshot_server|course-splits|course_scene|actor_nodes|world_renderer|render_threads)$' --output-on-failure

The new live test moves controlled frozen rider poses in quarter-sample steps
through the first Napa split and merge on both branches, with the real ring,
scene sinks and race camera update. It checks128568/131868 projected physical
samples, including all four dual/common transitions per branch, and447 rendered
views per branch. It checks absolute camera/origin consistency both live and
with a deliberately lagging interpolated position, selected-half retention at
the common-road handoff, and unchanged RNG during scene construction.

Foreground ground coverage:439 left/322 right valid lane probes, zero obscured
by terrain. Vertical billboards and RRRAIL guardrails are excluded by actual
texture identity/name, since they legitimately occlude ground. The assertion
allows one neighboring raster row at a strip boundary. An initial strict
single-row check found nine legitimate RRRAIL1 occlusions and one HILL12A
floor-mapped boundary at logical row170.403, not ten terrain spills; per-node
pixel tracing identified these before the assertion was corrected.

Local logs: build/live-split-probe.log, build/live-split-trace.log (initial
strict pixel diagnostic), build/split-final-tests.log, build/split-final-build.log.
Optional RR_LIVE_SPLIT_DUMP writes diagnostic native640x480 BMPs; inspected
previews show coherent roads/hills rather than full-screen inward hill spill.
Pure road tests check the original16-sample signed blend. Existing startup,
HUD/screen, reaction, projection, ring and snapshot regressions remain covered.

This is controlled native integration evidence and working-original disassembly,
not an original gameplay capture or a claim of complete original pixel parity.
No desktop replay, original process, installer execution or installation update
was needed for this correction. The earlier static split test remains component
coverage, supplemented by the new live-ring test.
