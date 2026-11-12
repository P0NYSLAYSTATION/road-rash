# Mixed contact order, pedestrian scenery and car teardown

Baseline8e66d09fedfda6e33435d56fb85d446a29ac4a5a is pushed and published1.1.14.
Public installer SHA165cfdeaa5f785c2dda87e0ed786af644949a6462555003f1fd8b585c7048e74
verified; isolated1.1.13->1.1.14 preserved600 files and passed SDL menu smoke.
Only E:/RR/ROADRASH.EXE with its working AWEMAN32.DLL is used. All diagnostic
captures below completed and restored the DLL. No original process is active.
E:/road-rash multiplayer WIP remains untouched. No agents were delegated.

## Current measured state

- Complete Hiway immutable14084-tick human race remains exact including RNG:
  build/mixed-contact-final-hiway.json, strict whole-race acceptance=true.
- Moving Napa2400 now exact including RNG; Napa expanded2400 and City signal600
  remain exact: build/mixed-contact-final-regressions.json (3 passes).
- Moving City matches through1558, first1559 car position34152/native34150:
  build/city-car-removal-store-order.json, strict2400 acceptance still fails.
- Focused traffic, pedestrian and car_contact tests pass:
  build/mixed-contact-focused-tests.log. Final MSVC build/SDL gate passes104 tests,35 explicit skips,zero failures
  in build/mixed-contact-final-full-build.log and mixed-contact-final-native-tests.log.
  No test/capture process is held. Direct3D device unavailable;
  exclude exactly directx9-main-window and integer-framebuffer as before.
- Upstream heads rechecked: main20fb4c0ac154378902132f6eb41a5e09804ebeaa,
  windows6b3b6be9ca5698b8b17f8f1ab53f2744a5202238, no new parity branch.

## Recovered behavior and implementation

Napa1259 movement was exact, then native applied a false car contact. Native
and original car height were both6147; height/order speculation was not the root.
Original4368f0's inner list stops at the first eligible node beyond the summed
half-lengths, before the lateral test. A crashed half at43273 has length16 and
flags7; rider4 at43171 has length64. Gap102 exceeds80, ending the scan before
the later car at43275, whose length64 would otherwise overlap. The old separate
car pass bypassed this termination.

A reach-only prototype fixed1259 but regressed completed Hiway10438 and stopped
Napa1293. It rescanned earlier nodes after contacts moved them. At1293 original
car43556 contacts rider4 at43609, moving that bike to43684, then continues to
rider9 at43658. Rechecking the already-visited moved bike falsely stops the walk.
That prototype was removed, never committed or released.

Live cars now share the existing ordered contact loop with riders, crashed
halves and pedestrians, and car static sweeps run at the outer car's turn.
Each eligible node is visited once in the original order. contact_car_nodes
reuses the physical mover images and common separation law; outer callback runs
before inner, preserving horn draws, crossing pauses, pedestrian hit response
and detached-half copy-back. Legacy unscheduled fixture passes remain separate.
The full Hiway race and moving Napa2400 both pass. Synthetic car test stages a
short eligible body ahead of a later longer car; lateral separation cannot
bypass the longitudinal early exit. Existing synthetic live car fixture now
provides the actual mover image rather than relying on the old separate pass
to manufacture its dimensions.

Original436952 calls the scenery sweep for pedestrians too. At City1251 a
standing type4 pedestrian changes state0->3 inside4373f0 with one BORR object.
The port skipped pedestrian scenery callbacks. It now dispatches00440760 for
both BORR and type2 markings, through the original outer-node midpoint gate.
Synthetic tests cover overlapping/nonoverlapping scenery and markings. This
advances City1251->1349.

City1349 crossing car lateral820346 exceeds0xc8000. Original4380ec marks removal,
setting next/last=tick+100;438113 then overwrites next with tick+interval. The
later longitudinal window test can mark again, keeping both+100. Native now
preserves normal next with delayed last for lateral/mover removal, and keeps
next+100 for a window exit. Original raw next1357,last1449 proves store order.
Synthetic test covers both removal reasons. Its first lateral fixture began
only1 unit outside and moved back inside; corrected to remain outside after
movement. City advances to1559 (two-unit car position difference).

## Original evidence

All controls validated, original exit0 and instrumentation phase restored:
- napa-matched-collision-state-1300.tsv SHA
  277e3337eceb0b385f684b71581482fa2ddf00ba3506e107581c6495f2dbc080.
- napa-pair-order-1300.tsv SHA
  88600e0b6773bdff6a74515db05695accdc4786b63d04e81e122bcbd812199b7.
- napa-contact-walk-1300.tsv SHA
  81868944633a801a9895803a262778bc8b06e0fc487e98a47c4edef91ae26df4.
- napa-contact-walk-1310.tsv SHA
  d52ca52b3e872b20bfed65c6020a8f08c7115751781bd1c3f387de5d65751c3c.
- City scenery proof: city-static-contact-1300.tsv SHA
  e14adb2f935ee780611ac3d0de2530574ed0f42a050bc61198438e63768282c2.

First Napa diagnostic without EXTRA_SPEEDS had different pre-race jitter tables,
so it was not used to infer the matched1259 geometry. Subsequent originals match
those tables without modifying live simulation/RNG. build/napa-matched-startup-speeds.json:
87,103,126,89,105,119,93,103,123,93,111,121,85,103,127,82,109,116.
City matching tables build/city-matched-startup-speeds.json:
85,108,126,85,112,128,82,101,121,87,110,123,93,103,117,81,108,119.
Course IDs: Napa4, City1, Hiway2 (read recorded environment, do not guess).

Optional comment probes verify original call sites4383b6->4368f0,
4369f6->436a40,436952->4373f0; helper builds with32-bit GCC. Native optional
RR_SIM_TRACE_TICK logs separation and static input. Diagnostics consume no RNG
and add no comparison exclusions. Source files currently changed: oracle proxy,
race.cpp/hpp, traffic.cpp; tests car_contact, pedestrian, traffic. Docs checkpoint
is being updated. No commercial traces/assets are staged.

## Next actions — continue autonomously

Final native gate passes. Update master docs/status, commit/push and build clean
installer1.1.15 plus isolated test installer. Validate update from1.1.14 preserves
600 files and SDL menu smoke; publish using outside v1114 scripts as templates.
Then continue City1559 and obtain completed Napa/City/Medly/Canyn recordings.
Use a conservative human input driver for sustained captures if needed; mode2
can wreck on harder tracks, and a wreck is not finish acceptance. Whole-race
validation requires UNTIL_FINISH and an outcome marker; do not edit traces to
add one. Multiplayer remains held until deterministic parity is complete.
