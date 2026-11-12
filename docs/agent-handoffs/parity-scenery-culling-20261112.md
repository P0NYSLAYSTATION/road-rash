# Scenery visibility and collision-box lifetime

Baseline c8f72b6, codex/simulation-parity. Latest published installer1.1.4.
Only E:/RR/ROADRASH.EXE and the working AWEMAN32.DLL are used for captures.

The completed14,084-tick Hiway original fixture is unchanged, SHA256
0fe2e9d7dd984990c528ee4b05cf80015c37b2e7a84b2c442e0b524e27d5975d.
Strict replay now matches ticks1..6603, including RNG, without reanchoring.
First gap6604 is a parked scheduler bike/half at223502 original versus223501
native. The original rider slots do not expose this parked actor's live fields;
node-list comparison detects the difference. Parked pace/timing capture is next.

## Recovered behavior

At6557 native movement already matched steering2310/rate1080. Contacts applied
a false type11 decal at sample952, selector133, lateral650 and box
[-6656,-4096,8704,4096], producing steering3465/rate0 and changing the decal
animation words. The original's same billboard had zero loaded boxes.

00444553..004445d0 marks the half opposite the camera invisible using a
depth-indexed separation margin and signed split/merge countdown. The camera
on a common road retains both upcoming roads except for the merge-event path.
Events4/6 read the current slice's countdown; other events read the camera's.
The building-terrain condition reads physical half0 at the camera.00444e70
builds32 world-space separation limits from the pixel margin table4734a0 and
fixed depth scales starting46f498, stepping0x200 bytes.00443b40 skips invisible
road contexts before00438610/0044b980 can load billboard collision boxes.

Native box preparation previously marked both roads loaded just because they
were inside the camera's forward range. It now applies the original half-cull
rule using physical ring countdown/event state before latching boxes ready.
The gate is evaluated on simulation ticks, independently of renderer frequency.
This corrects6557; it does not establish complete lazy-load parity for every
scenery or pedestrian object. Rendering's own secondary-road cull and remaining
per-object/rail range gates still need integration and evidence.

## Evidence

- `hiway-static-boxes-6600.tsv`, SHA256
  c25dd29fe3144dcb69061d41e7539a02b61bdd8f54bebdfcb111126ce1d5f39d:
  original sample952 billboard count0 through6557. This older probe mislabeled
  selector/flags as type/selector; interpret entry+0=selector133,+1=flags2,
  +2=type11,+3=box count0. The next probe corrects its labels.
- `hiway-static-visibility-6600.tsv`, SHA256
  f4319e472c9b326170852c27f52f0e803e22c4d09d0294a8118f39a62b1a48fa:
  the same raw slice4889124, local_q8=13312, half1 invisible throughout6477..6557.
  At6557 projected shoulder columns137386744/140789080 are far outside640px;
  no billboard draw dispatch occurs and box count remains0. All6600 controls
  validate and the working DLL is restored by the capture receipt.
- `build/hiway-6557-billboard-contact.json`: native contact-phase evidence.
- `build/hiway-scenery-road-cull.json`: completed trace exact through6603,
  first6604 parked-position difference; command exits1.
- `build/scenery-road-cull-regressions.json`: Napa2400 and City600 exact.
- `build/scenery-road-cull-native-tests.log`:141 registered,106 passed,
  35 corpus/platform skips, zero failures. Build:
  `build/scenery-road-cull-full-build.log`. Pure gate tests cover strict
  margin equality, common-camera retention and the two countdown paths.

Independent moving Napa1259 and City1251 gaps remain; Medly's earlier wreck
is not finish acceptance. All-track start-to-finish parity remains unfinished.
