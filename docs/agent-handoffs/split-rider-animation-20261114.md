# Split terrain and opponent animation — 2026-11-14

Baseline: Windows branch `port/plan33b-windows`, `aae2c9f`.

## Split rendering

The previous inward-hill correction was restricted to approaching merges.
Working-original E:/RR/ROADRASH.EXE disassembly confirms that 00443d5f calls
0043be80 for every dual roadway. Positive split countdown dispatches through
0041d500 to flat separator 0043bb30, rather than extending inward hills to the
viewport. The separator has 12-world-unit edge strips (palette E5), centre F7
when separation exceeds24, and 0043bdf0's depth210 near clip. These are now
implemented. The existing route's separation and inner shoulder widths recover
00453b60's threshold (first clearance+256 separation, cap80); inward polylines
use bounded bands beyond that threshold. The zero-countdown ridge is flattened,
matching existing 00454890 terrain-ground behavior. Closing a departing dual
window now stitches the selected near half to the following single road.
All changes are local draw construction; stored simulation terrain is unchanged.

A separate actual Napa branch-B pose (sample768) exposed an exterior HILL13A
quad whose mapped X exceeded signed16-bit range. Narrowing approximately
-50,826 pixels to int16 wrapped it onto the opposite side of the screen.
Original 0040ffc0..00410075 stores all eight mapped coordinates as DWORDs.
Both indexed renderers now use their existing wide-coordinate raster path for
such polygons at native resolutions as well. Ordinary in-range polygons keep
the existing original-resolution arithmetic. RGB already retained wide vertices.

The departing-split regression probes valid lane interiors on both Napa routes,
across all three splits, before/inside/after each window:86 valid probes, zero
covered by terrain. Billboards are excluded by their actual imported texture
identity; zero-width road spans are not lanes. Early diagnostic centre probes
hit RRSSTA textures at spans with no lane width and a WTRP3SM2 roadside sprite;
those were not treated as terrain bugs. A synthetic -50000px hill verifies that only its
left-screen sliver is painted and the roadway centre stays untouched, in both
single-threaded and four-thread indexed rendering.

## Animation ownership and lean

Game maintained and advanced a second animation block per rider, independently
of Race::ride_animation. It reconstructed attack starts but never received the
victim's original damage reaction9 or the AI's directly selected poses. Removed
that duplicate controller and bookkeeping. Drawing now uses the simulation's
existing selected descriptor, including its scheduler cadence and queued poses.
Original damage path004052c0 and existing knock_off/resolve_hit already select
reaction9; no combat mechanics or simulation RNG were changed.

Original00446484..004464b7 tests the rider's own drawn heading (+68), then clamps
selected body roll to[-50000h,+50000h] when abs(heading)>e0000h. Native instead
tested camera-relative yaw and forced either +50000h or -50000h, suppressing
cornering lean and sometimes choosing the wrong direction. ActorNodes now uses
the original heading test and preserves the roll's sign and smaller magnitude.
Four real-bank body-node cases cover camera yaw versus own heading, both roll
signs, zero roll and small roll, checked against explicit roll overrides.

The reaction integration test uses a controlled two-rider flat-road duel with
real imported bike/physics/animation tables. Narrow collision boxes isolate the
punch from physical separation; damage, attack reach, original animation
selection, Game::tick and the actual rider pixel renderer remain active. An
actual landed punch reduces stamina and selects reaction9; its pixels differ
from the neutral pose. A simulation-selected AI gesture likewise reaches pixels.
This is controlled integration evidence, not an original-game gameplay capture.

## Verification

MSVC Release builds affected targets. Focused CTest passes13/13, zero failures
or skips (47.39 seconds): race-screen, race-rider-reactions, race-start-camera,
race-crest-canvas, actor_nodes, world_renderer, course-splits, course_scene,
scene, actor_descriptors, actor_render, actor_animation and render_threads.
The original actor component fixtures remain passing; course_scene retains its
all-five-course/all-route construction sweep and existing merge/tunnel checks.
Scene tests additionally check separator colours, widths, rows, narrow-gap
selection, exact depth210 clipping and the zero-countdown ridge.

Evidence: build/split-reaction-build.log, split-components-build.log,
split-reaction-tests.log, rider-reaction-test.log and split-regression.log.
Read-only original disassembly and ignored local native diagnostic images were
used; no original-game launch, desktop replay capture or installer run occurred.
Full original scene pixel parity beyond these fixtures is not newly claimed.
