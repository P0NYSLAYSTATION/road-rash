# Projected traffic velocity and full-race captures — November 12, 2026



Baseline e5d0de9 (published 3do-1.1.15). Owned source: src/traffic.cpp,

tests/traffic_test.cpp, oracle/aweman_proxy.c. No parallel workers.



## Measured correction



City's first mismatch at1559 came from the leader car at33258: scalar speed386,

projected +108=-382. Original0044e96e reads +108 and negates it for kind2 before

0044e980..995 converts to target units. Scalar386 yields3, projected382 yields2;

with gap58 and bias-45 the resulting target is1600 versus1500. Native car at34186

therefore ended34150/speed1534 instead of34152/speed1440. Other-car obstacle

velocity now reads physical +108/+10c, matching the original. Unchanged City

2400/2400 ticks now match including RNG: build/city-projected-velocity.json.



Synthetic test_follow_projected_velocity verifies scalar changes cannot alter

following when projected velocity is fixed, and crossing the projected threshold

does change following. First synthetic setup inherited lane_sample's offset1000,

so its obstacle was outside lane-1; corrected to a straight road centred at0.

Focused traffic-test passes (build/city-projected-unit-test.log).



## Capture tooling



Optional driver4 supplies conservative human acceleration/brake/steering inputs,

reads physical road edges +e0/e4 and predicts lateral drift from+10c. It does not

write simulation fields or consume RNG. Brake refreshes original keyboard axis1.

Controls are recorded for a single-anchor replay. This is testing input only,

not a game simulation change or a claim of original recorded demo inputs.



Original napa-conservative-full-race completed29055 ticks, outcome3 (finish),

seed1/level1/human. Trace SHA88182a0ca77be490a2ef1789211debaa76aa001e492e245f7f113caa2f56350c;

controls SHAd2c8d977d16c748509f0039b04180284a511e8b5ea1f0f40618db7034bc3880e.

Receipt confirms working AWEMAN restored. New full replay initially diverged2111:

car and rider shared position41825; native excluded equal-position obstacles,

while original00449d1b/64 tests only upper reach in the ordered list. Accepting

zero gap advances exact prefix8191. Synthetic traffic regression now covers

zero gap versus a node one unit behind (not yet rerun).



At8192 a respawned police rider's initial target_speed was120/native0.00405da3

calculates its first target before replan, using persistent personality speed.

The new cop path retains previous current_speed/current2 through teardown and

uses these before the ordinary spawn replan; pursuit speed still gets overwritten

at the original later point. Exact prefix advances11334.



At11335 a half0 car latched an intersection signal on half1. Original00449d30

rejects disjoint slice roadway masks. Native signal lookup now checks controller

half against the car's route half. Exact prefix advances16583. First16584 is

ring.340de28.lk18_2c original256/native257; dynamic lane widening at a merge

requires tracing. build/napa-signal-road-filter.json. No golden fields changed.

New synthetic opposite-intersection test checks half0 rejects/half1 accepts.



Hiway complete14084 and four short regressions pass on projected-velocity-only

source (build/city-velocity-hiway.json, city-velocity-regressions.json). Native

full build/gate at that point passed104 with35 skips and two D3D exclusions.

Subsequent equal-gap/cop/signal changes still need these regression gates.



## Held resources and next actions



Original City capture city-conservative-full-race is running under exec70636,

working E:/RR DLL instrumented; do not rebuild/replace capture helper or use GUI

native tests until restoration. Mode4, seed1/L1, COURSE1, matched City extra

speeds, max70000/timeout1500, UNTIL_FINISH1. Last observed tick17273, position228591,

condition83, crash0. Only its complete receipted output may establish acceptance.

Native merge-lane probe exec63297 is running at16584 with RR_FIELD_ALL; wait before

relinking oracle-sim-test. No parallel agents or other owned resources.



Continue merge lane correction, full Napa parity, then longer City/Medly/Canyn

and other branches/levels. Multiplayer remains held. After a measured checkpoint:

final full build/SDL gate and strict regressions, master docs/status, explicit

commit/push, clean installer1.1.16 and isolated update from1.1.15 preserving600

files/menu smoke, publish. Baseline1.1.15 was published and public checksum verified;

its previous handoff's publication step is completed. Templates are outside repo

Documents/New project/publish-v1115.py and verify-parity-v1115-upgrade.py.



## Napa complete acceptance



After event4 suppresses lane widening (00455155 skips4547e0), exact Napa reaches

29014. Original then continues its scheduler during the result-screen delay even

though every rider has finished; the native all-finished guard froze the race.

Live simulation now continues that delay and physical motion substeps; standalone

fixtures retain their finish behavior. build/napa-result-delay.json reports

acceptance_met=true: all29055 ticks identical including RNG, single seed anchor,

no reanchor and no golden edits. New synthetic live-finish test checks that the

scheduler advances after the last rider finishes. Gate still pending latestcode.



City original completed33819 ticks, outcome3. Trace SHA

f3883ed9b2c3d50819978bcd1a22e13c2b6fdd058ea449f660b92a8b4bb1fe4b;

DLL restored. Replay first6207 ped.3136474.28 original-187904/native-192215.

City probe exec52403 is in progress; no relink until it completes.

Medly original capture exec27913 now owns E:/RR DLL (mode4/COURSE3/seed1/L1,

max70000/timeout1500/UNTIL_FINISH, no extra-speed override); do not replacehelper

or run GUI native gates while held. Earlier resource notes are superseded.


## Subsequent City and Medly evidence

City6207 original clamps pedestrian lateral to-187904 at the shoulder; native
kept walking to-192215. Verified working EXE0043adf1 calls0043a610 before vertical
settling for type0x20 too. Added shared walls(p.body) between movement and vertical.
City advances6808, first6809 car original79412/native79414. A synthetic pedestrian
boundary regression passes both shoulders with E:/RR assets. Initial direct
pedestrian-test invocation without RR_ASSETS printed corpus absent and ran no
pedestrian tests; rerun with the working assets passed the actual checks.

City6809 exposes why equal-position scans must preserve list direction: car0's
unordered list selects the player's equal-position bike, sets gap0 and suppresses
the red-light stop line at gap2. Original forward links do not include that bike
at this tie. Added live TrafficSystem obstacle-scan callback using Race's existing
mixed ordered list, shared-road masks and original scan_next. Standalone systems
keep scalar fixtures; live even-kind cars now honor linked scan direction/ties,
including noncrashed halves and roadside nodes. Strict City/Hiway/Napa checks
are running; do not assume these pass until reports complete.

Medly capture completed29172 ticks but ended in arrest (outcome4), not finish;
SHA3ff1869c462d273764a0aebcccb6d9967d9df82b592af349135caef5eace2bb1.
The game was nearly stopped at the end, condition42; this is not finished-track
acceptance. The tool restored E:/RR DLL and no original process/resource is held.
Need improve driver safety/recovery or use another seed for a finished Medly run.
Full City is finished (33819), with parity still under repair. Latest full pair
before ordered-scan change: build/projected-final-whole-races.json passes both
Hiway14084 and Napa29055 including RNG. New helper mode4 itself produced two
finished races without simulation writes. Driver2 Medly had previously wrecked.

## City advances to18307

Ordered lateral scans now reproduce00449da0: search both longitudinal directions
within0x80, retain the nearest candidate on the requested lateral side, then
0044a070 applies its same-sample check. Return projected +108/+10c. This fixes
8594 and advances9502. Shared walls now resolves retained physical ring slots;
0043adf1 sees rebuilt geometry on a due actor's last out-of-window move, fixing
City7466. Its speed integration must also read that rebuilt surface:9503's47-tick
step uses drag6 (original518), not historical drag5 (native565).

At8391 original0044f00c sets a default60-tick crossing-car pause for any contact,
then0044f027 raises it to240 only for bike/half types. Live car callbacks now do
both; synthetic car-to-car contact test passes, retaining existing bike240 test.
Synthetic opposite-road signal test, projected-speed/equal-gap tests and live
all-finished scheduler test pass. Updated pedestrian fixture actually ran with
RR_ASSETS=E:/RR and includes both shoulder boundaries.

Final City direct diagnostic matches18307; first18308 pedestrian resources original
unloaded/native loaded: serial59/selector193/position251980/lateral281600.
build/city-ped-physical-drag-diagnostic.log. This likely involves the explicitly
unported projected-x draw visibility test; investigate after releasing checkpoint.
Direct oracle binary runs are quicker diagnostics on already validated immutable
traces; strict verify_sim still validates receipts and full coverage for acceptance.
All original processes restored; no original DLL holds. Full native gate7553 and
strict full Hiway/Napa24459 are currently running. Do not change code until gates
and1.1.16 release complete. Prepared outside scripts publish-v1116.py and
verify-parity-v1116-upgrade.py. First multi-target focused build used unsupported
comma target; no tests ran, then individual targets built/passed. One compile used
nonexistent HalfMirror.lat10c; corrected to lat_vel and rebuild succeeded.

## Checkpoint gates complete

build/v1116-hiway-acceptance.json passes both strict whole races: Hiway14084 and
Napa29055 including RNG, single-anchor, original finish outcomes. Four prefix
regressions pass in build/v1116-release-regressions.json: moving Napa2400,
expanded Napa2400, moving City2400, City signal600. Full MSVC Release build and
SDL CTest gate pass104 with35 explicit corpus/platform skips,zero failures and
two unavailable Direct3D tests excluded. Logs projected-final-full-build.log and
projected-final-native-tests.log. Focused actual-corpus pedestrian checks pass;
synthetic traffic/race/car-contact checks pass. A final null-controller guard in
the optional traffic trace is included for the clean build. No simulation edits
remain pending for this checkpoint. Next commit/push, clean installer1.1.16,
update validation from1.1.15 and release; then City18308 and remaining captures.
