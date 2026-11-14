# Rendering parity milestone and next work — 2026-11-14

Checkout E:/road-rash, branch port/plan33b-windows. Baseline131a54e/latest release
1.1.30. Owner explicitly reconfirmed standing authorization to build, commit,
push and release for this conversation. No further permission request needed.
Original ONLY E:/RR/ROADRASH.EXE and working AWEMAN32.DLL. No subagents. Menu
pixel parity/widescreen-only issues excluded. Do not execute installer.

## Validated milestone, ready to commit/release1.1.31

Missing original81-rise branch initialization now applied at course load.
Graph/physical half forward links correctly cover both split and merge reentry.
Shared two-road cumulative occlusion ceiling, event6 inner phase, exact original
band limits and floors/exteriors/inner-gap painter sequence restored. Live ring
retains original band metadata; inner shoulders dispatch independently of floor
visibility. The two original right-entry failures now match486 and275 terrain
polygons with zero missing/extra/order differences. Side-specific far projection
was the final erroneous shoulder over the lane.

Whole-route posed Napa sweep:6134 frames per branch,24847+25368 lane probes,
zero covered lanes. Imported route continues beyond finish: diagnostic explicitly
disables finish handling, traffic/pedestrians/spawn/scenery contacts and asserts
no crash/result pullback, instead of misinterpreting finish camera as geometry.
Actor membership uses drawn sample/lateral, parked/null-slice actors invisible,
scenery/fences interleave by original lateral ranges. New actor-placement case
registered. Pedestrian natural-race callback also accepts/applies lateral range.

Original capture now hooks all seven direct flush sites. Final capture ignores
pending-list flag. Posed components service and settle actual asynchronous MSRR
queue so accelerated traversal cannot overrun it; NOT used for sim acceptance.
Capture full version2; comparator rejects old incomplete full captures. Solid
colour texture flips normalized. --ordered-terrain checks order, not just sets.

Seven registered CTests compare five settled course800 windows and two Napa
entries; allpass including fresh grounds. Terrain counts:Hiway197,Canyn282,
City226,Medly244,Napa143,Napaentries486/275. Four complete ORIGINAL display-list
raster replays (Canyn302/City236/Medly268/Napa149 nodes) have zero unknown textures
or differing pixels. This does NOT verify native scenery/actor construction.

Native full Release build passes(build/render-milestone-full-build2.log).
44-test focused gate43passed/cachecrashed on stale test read of presentation RGB
rather than retained world RGB. Repaired test checks horizon rows in world_rgb,
zero warm allocations and retained lower world rows; all12 configurations pass.
Final registered original/cache gate8/8 passes(build/render-milestone-original-gate.log).
Strict five completed human races143764ticks including RNG pass one-anchor,
no skips/divergences/failures(build/render-milestone-five-finishes.json).

## Immediate next actions

Commit explicit owned paths to WINDOWS branch, push. Rebuild clean rr-game so
build metadata dirty=false/currenthead. Build installer version1.1.31 with
python tools/build_fork_installer.py --version1.1.31 (space before value).
Adapt ignored build/publish_v1130.py for1.1.31; four assets only, same-accountfork
P0NYSLAYSTATION/road-rash. Release notes describe actual changes/evidence and
remaining limits; no irrelevant branch/release history. No commercial assets,
no installer execution. Then CONTINUE toward full native race rendering parity.

## Next rendering slice after release

- Full-list actor-record indices still use tail-local nbefore/nafter in
  oracle/actor_record_hooks.h. Add optional absolute full-node positions while
  preserving existing synthetic fixture hooks. Comparator --native-actors must
  use absolute indices in full captures. Do not claim those full captures pass
  current actor mode without fixing this first.
- Compare native scenery/fence/actor list construction/order against actual
  original inputs, then full indexed frame pixels and multi-frame native race
  scenes. Existing raster mode replays ORIGINAL nodes over original pixels;
  it proves raster component parity, not native scene reconstruction.
- Broaden terrain components across hills, both arms, entrances/exits, levels
  and courses. Later forced-half posed traversal can differ in branch history;
  verify physical identity before calling actual native/original input parity.
- Source text and tests changed in this slice are owned; no other agent work.
  git diff --check passed. Local commercial traces/raws stay ignored.

No active original capture/owned game process or held resource. Last completed
sessions:originalgate42388, fullbuild/sweep21588, fivefinish66025. All original
captures restore the working DLL automatically. One startupAV was rejected;
subsequent complete captures valid; a full-mode exception logger now flushes
actual fault registers if it recurs. Do not reuse rejected captures.
