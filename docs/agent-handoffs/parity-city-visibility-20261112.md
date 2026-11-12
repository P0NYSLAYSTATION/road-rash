# City visibility continuation — November 12, 2026

Baseline029ff0844592ee8558ab37fa2fe2dad781d44082, pushed/released3do-1.1.16.
Public checksum1f84fb0addad65b909abf28ad36c3ad9ee372e22102cebb53899a0a31c108bbb.
Clean MSVC/SDL gate104passed35skips2D3Dexclusions; update1.1.15->1.1.16 preserved600
files and SDL menu smoke; public download checksum verified. No publication gap.

City immutable finished33819 capture (f3883ed9b2c3d50819978bcd1a22e13c2b6fdd058ea449f660b92a8b4bb1fe4b)
now exact through18307. First18308 ped serial59/selector193, originalunloaded0/native1.
Original first loaded at tick18310. Native loads after tick18307 because samplebase
251904 <= ringcamera244235 +7680. Ped actualpos251980. Original ringcamera244265
at tick18309. Current approximation may have a fractional reach mismatch, or the
original may have run multiple simulation ticks between rendered frames. Existing
capture stores simulation ticks but no render cadence; neither cause is proved.
Do not call the latter a finding without fresh evidence.

438450 copies raw object+1c..2c into+44..54, so4386ef's +50 check is raw world
lateral (truncated/256) against caller bounds, not projected screen x. Earlier
comments call this a projected-x test and need correction when its actual role
is established. Normal438610 bounds are[-10000,10000); rail variant443c00 divides
into three ranges at shoulder edges. Original438450 collects max50 drawn actors,
actual positions from camera+0x12 through(camera&~255)+0x2000. Need trace actual
render/root association before replacing these rules with a guess.

Only new edit so far: oracle/aweman_proxy.c optional DRIVER_TRACE now also runs
with recorded-input playback and appends g_frame render counter to each existing
comment line. No simulation fields/RNG changed. Do NOT build helper while Canyn
capture owns the existing helper/DLL. Once restored, compile and run matchedCity
recorded controls to18330 with new render comments. Original inputs are
analysis/oracle/city-conservative-full-race.controls; EXTRA_SPEEDS City table in
previous handoff. Use COURSE1,seed1,L1,human,DRIVE0,INPUTabsolute,DRIVER_TRACE1,
FRAMES18330, RECORD1,RANDLOG1, no UNTIL_FINISH (diagnostic prefix only).

Canyn original exec8282 currently owns E:/RR DLL, started after release/menu smoke.
COURSE5,mode4,seed1/L1,max70000,timeout1500,UNTIL_FINISH1,noextraspeedoverride.
Last observed16754 position281343,condition142,crash0. Wait/restoration receipt
before using output or replacing helper. No other process/resource ownership.

After City diagnostic continue fixes; obtain finished Medly recording (last mode4
run was arrestedoutcome4 at29172, notfinished); Canyn full parity and other levels/
branches remain. Multiplayer held. Continue autonomously, no questions/endstatus.

Prepared the render-cadence helper in a separate output directory
oracle/build-windows-renderprobe/AWEMAN32.DLL (GCC32 build succeeds; expectedformatwarnings).
The running Canyn helper path and installed E:/RR DLL were not replaced. Use this
separate helper for the next City diagnostic after Canyn restores. Previous
prohibition refers to replacing/rebuilding its held helper, not an isolated build.
Medly arrested-run replay first3759 car ring.318d42c.position44559/native44549.
Direct diagnostic logbuild/medly-arrest-first-parity-diagnostic.log; nativeprobe21838
is running. This remains diagnostic coverage, not a completed Medly race.

Canyn completed29620 ticks, outcome3, trace SHA
7ae419ba5784a62d7cb12b4b4abff9e7f6ae03e10bca5dd6fefe8c8cdef2eacc;
DLL restored. Release1.1.16 binary direct replay all29620 identical including RNG.
Strict whole-race Hiway/Napa/Canyn check45386 runs against the next source fix.

New src/traffic.cpp: crossing invalid-zone gate now suppresses think only, still
executes independent physics mover using retained motion heading. OriginalMedly
3759 markedlast3859 yet moves44549->44559, lateral233712->200272, speed1762->1520.
Original44dc10 marks,44e710 returns, but scheduler still invokes separatelyinstalled
mover43adc0. Focused synthetic marked-crossing test passes after removing an
incorrect friction expectation (synthetic_exe has zero surface drag); it verifies
lateral motion and retained heading. Medly now exact10160; first10161 pedestrian
heading original4193664/native127. Original440981 really does write bare127 on a
nonpositive bump, so do not change angle units as a workaround. Need trace why
native bumps this pedestrian while original retains its crossing heading.

City recorded-input diagnostic exec91965 currently owns E:/RR hooks. Course1,
matched City speeds, seed1/L1, DRIVE0, INPUTcity-conservative-full-race.controls,
DRIVER_TRACE1, frames18330, isolated renderprobehelper. No UNTIL_FINISH; this is
only a diagnostic prefix. Wait restoration receipt. Native Medlyprobe is running
for10161. No nativeGUI gate while original capture is active.

Strict whole-race gate completed: build/v1117-whole-race-acceptance.json has
acceptance_met=true for Hiway14084,Napa29055,Canyn29620 including RNG. The next
checkpoint has three measured completed courses; City/Medly unfinished.
Medly10161 native diagnostic shows a BORR contact (types32/1), at sample539,
actor138100/lat-143360, static138112/lat-136192. First native field differences
are heading127 and state3; original keeps heading4193664/state0. No simulation
fix yet. This is not a wrong angle unit:440981 explicitly writes127.
Possible retained static-box/profile mismatch needs original10161 contact probe.

City cadence diagnostic completed18330, restored DLL, SHA
8aafcef00e7bdbebcf4f54cef644124b441858111a76568ca2de776a7a8b3b01.
At18300..18319 g_frame advances exactly one per tick; player position/lateral,
RNG and controls at18308 match the original full capture. Native reproduces the
same18308 graphics-availability gap. Cadence is ruled out for this fresh gap;
do not keep reporting it as the cause. Need actual438610 world-strip visibility.
Optional read-only rr_draw_half_probe added to four verified direct438610 call
sites443b6d/bc2/bf2/ca2, enabled only with AI_TARGET_TICK. Logs world bounds,
retained slice identity and matching pedestrian render-queue entries within±4ticks.
Compiled to separateoracle/build-windows-drawprobe/AWEMAN32.DLL, not installed yet.
Use it for next matched City diagnostic target18308, frames18330, after current
Medly run restores. Native temporary ped-load diagnostic added inload_pedestrian_looks
for RR_SIM_TRACE_TICK; nativeexec72538 runs18307 againstfreshCity prefix. Remove
this temporary native print after diagnosis, or document if retained.

Current original owner exec23803: medly-conservative-full-race-s2, COURSE3,
seed2/L1/mode4,max70000,UNTIL_FINISH1, DRIVER_TRACE1 with renderprobehelper.
This seeks a finish with a different declared seed after seed1 ended inarrest;
both original and native will use the same new seed, no golden edits/reanchor.
No original other than E:/RR. Do not install hooks or run GUI gate untilrestored.
Need nativefullgate/short regressions, commit/push, clean installer/update1.1.17
once this checkpoint's tests/probes complete. Three full acceptance receipts
already pass on marked-car fix (v1117-whole-race-acceptance.json).

City native ped-load probe18307: serial59 sample984, pos251980, lat281600,
ringcamera244235, viewcamera243979, route0/node20/half0, view.dual=false,
countdown0. Thus not an opposite split-road visibility case. Original drawqueue
eligible raw positions at this tick include only3peds,1car,3rider nodes, well
below50; queue capacity is not a credible cause. Actual dispatch/root identity
still needs original drawprobe. Added actual/copy camera globals to its comment.
Read-only raw BORR boxes/slots/actor dimensions also logged by existing
rr_static_sweep_probe for a future matched Medly10161 diagnostic.
Matched Medly speed table is spd_b (+1bc), not spd_a (+1a8); corrected local
build/medly-matched-startup-speeds.json before any capture used it:
93,113,125,84,111,122,91,111,117,91,103,127,89,106,121,90,104,119.

Medly seed2 last observed36563, position457137, remaining8271, condition1/crash0.
Wait actual outcome, do not count as finish yet. Added optional driver5 to source
and built isolatedoracle/build-windows-lane-driver/AWEMAN32.DLL; it biases to the
forward lane (origin+125, clamped64 units inside pavement) and lowers targets to
3500 straight/2500 curves. Motivation: midpoint can be an oncoming lane on an
asymmetric road. Mode5 has not been run; keep it uncommitted until validated or
remove if seed2 reaches the finish and it is not needed. Existing mode4 captures
remain unaffected, as their installed helper and source traces are unchanged.

Next original command after Medly restoration: City drawprobe, existingrecorded
controls/matched City extra speeds, AI_TARGET_TICK18308, frames18330,
helperoracle/build-windows-drawprobe/AWEMAN32.DLL. This must establish actual
438610 root/bounds/queue behavior; fresh City cadence already rules out dropped
renders around the discrepancy. Captures only atE:/RR/ROADRASH.EXE.

## Current checkpoint — three completed courses

Medly seed2 completed37186, outcome3 (finish), trace SHA
61677b5a79fcfd227faa6a7c5433ef7a535163d8a0d9a81730bcca533ceb6a77;
DLL restored. Direct native replay initially failed13479: newborn serial36's
retained crossing lane original-1/native0. Original437df0 returns marked nodes
in ascending linked-list order; native returned in storage-slot order. This
changes LIFO reuse/retained fields when multiple slots expire together. The
prepass now returns marked pedestrians by the existing mixed list before this
pass clears its sort keys. New synthetic test reverses slot/position order and
checks the next spawn retains the last linked node's lane. It passes with actual
E:/RR corpus. Initial prototype rebuilt scan after clearing car keys on every
pass, regressing11351; rejected. Conditional prepass at the original point
preserves keys and advances exact Medly seed2 through20645. First20646 is a
resource availability difference (serial57/selector128/pos255054/lat140800).
build/medly-prepass-key-order-diagnostic.log. Hiway/Napa/Canyn direct full
regressions remain identical. Mode5 was never run and has been removed; the
successful Medly seed2 still used unchanged mode4. Temporary nativeped-load
print has been removed. Sourceprobehelpers retain only read-only diagnostics.

City drawprobe completed18330, SHA
52b2fd41e1af1fee5c523ff15356bc1b06c0247b566ee69ce6cbe124608c78aa,
restored DLL. At18307 serial59 is in queue8, matches its dispatched slice4913088,
world lateral1100 inside bounds[-10000,10000); loaded is0 beforedispatch. Same
at18308; by18309 loaded1. Therefore missing root/queue membership or opposite
road visibility does not explain this gap. Need inspect actual family lookup
returns (440694/4406ac ->44acd0) and post-dispatch state. Original44a1a0 is a
simple state guard, not a Windows wait; do not claim async/mutex latency without
additional evidence. Earlier first-load tick notes were off by one; use actual
capture tick fields/transition for comparisons. Both Cityprefixes reproduce the
same original state and native discrepancy. No current originalprocess orDLLhold.
Next probes must use E:/RR and unique scenario names; can investigate resource
lookup before modifying native timing. Also original Medly seed1 BORR mismatch
at10161 still needs raw-box evidence if it recurs on the finished-seed2 scenario.

Full nativegate10512 and strict Hiway/Napa/Canyn10324 are running for release1.1.17.
After pass update docs/status, commit/push shortmessage, clean build/installers,
validate update from1.1.16 preserves600/menu smoke, publish. Then continue City
resource lookup and finished Medly seed2 resource gap. All-five finished original
recordings exist; strict parity remains3/5 completed scenarios, not all levels or
branches. Multiplayer remains held. Continue autonomously, no user questions.

Latest checkpoint gates completed: strict Hiway14084/Napa29055/Canyn29620 all pass, four prefix regressions pass. Native Release build and suite:104 pass,35 explicit skips,2 unavailable Direct3D exclusions,zero failures. Receipts v1117-whole-race-acceptance.json (09:22:59), v1117-release-regressions.json (09:23:25). No original capture/process held. Proceed release1.1.17 then continue resource lookup diagnostics.
