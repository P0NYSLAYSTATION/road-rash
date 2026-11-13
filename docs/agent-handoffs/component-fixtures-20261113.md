# Original component fixtures and required checks

Baseline4924b2b (pushed). A broader stdout audit found three false-success exits
(actor_render,crash,actor_traffic) on absent fixtures. Static inspection also
found silent optional fixture blocks in actor_animation,actor_descriptors,
building_panels and the police constructor/arrest checks. Earlier audits which
reported only menu exclusions searched skip markers too narrowly. The old
CTest pass counts did not establish these missing component comparisons.

Generated data from the exact working E:/RR installation, never a reference
installation or another runtime location:

- `napa-native-component-fixtures-20261113.tsv`:one rendered frame, complete,
  SHA2567f492718f0b17908013f5d5ae6b23a8ac478bc5ef096743a59594a3fdbf1d433.
  Original function-driven fixtures, not a simulation replay or human-driver
  acceptance. Contains actor,vehicle,crash,building and arrest cases.
- `napa-native-police-constructor-20261113.tsv`:two ticks, complete,
  SHAcc01277945b9d1bb890eaa65504e48816c9f523d0406a59d39b546698dc6e129.
  Explicit constructor probe; its artificial police spawn is not ordinary
  gameplay acceptance. The initial attempt was rejected because constructor
  observations preceded the frame marker. Capture now buffers those fields
  without moving the constructor call, emitting them inside the next frame.
  Failed trace retained under failed-captures/capture-0b42c7f811a94a0295837ed5a14c14ec.

Both capture runs restore the user's working AWEMAN32.DLL, verified
ca7f2f65bf6d1550857556a27d65d4f44d7a6ca14c2be89b7c0f9affb6a47022.
No original process or capture claim remains active. All derived data stays
local and git-ignored.

CMake now supplies explicit cache paths to the component tests. With supplied
assets, missing required fixtures fail; absent commercial assets report77
with disc/fixture labels instead of silently passing. Crash walking and police
spawn/arrest are required rather than optional blocks.

`build/v1120-required-components.xml`/`.log`:six tests pass with actual comparisons:
2048 animation steps; actor descriptor/crash/riding/view sequences;256 actor
projection cases;256 walking,64 remount,256 recovery,512 actor motion and64
crash-entry cases;1024 arrest cases and original police constructor;207 vehicle
graphics cells,30 descriptors and1440 view selections.
`build/v1120-missing-fixture-rejections.json`:all six executables return1 for
an intentionally missing fixture. `build/v1120-ground-building-components.xml`:
building128cases/145nodes and128shoulder cases/429nodes;terrain2112original
ground queries;CEL42resources all pass. Terrain/CEL already used configured
original fixtures; building's silent optional block is now required too.

No gameplay implementation changed in this follow-up. Replay executable stays
SHA25654c3f0bd2b3bc205cf4d23b4a46cd5590533341ebb7fad5410fb2a7620139a48,
identical to the clean4924b2b binary which passed all five complete races,
Medly wreck and94929tick Level5 arrest recording. Reuse that measured evidence
only while the binary digest remains identical.

Release1.1.20 is NOT published. Its earlier installers point at4924b2b and must
be rebuilt after this test/tooling commit, followed by the19-to20 upgrade test.
Current XML `v1120-native-tests.xml` is the pre-fixture439.96s gate:153CTestpass,
0CTestskip but missing internal component coverage, so NOT a valid final release
receipt. Preserve it before the final configured full gate. New project scripts
prepare audit/publication; audit ignores JSON skip:0 counters but rejects actual
absent/skip messages outside owner's menu exclusions. No pending builds/runs.
