# Native camera and composed race rendering — November 20, 2026

Checkout `E:/road-rash`, Windows branch `port/plan33b-windows`. Baseline `936dff8`
was released as1.1.33. Test-only native-camera work was pushed as`d384da2`.
This continuation fixes extended Napa rendering and retains Highway acceptance.
No new branches, installations or installer execution.

## Capture constraint and retained evidence

The owner prohibited further original captures after the active Napa run.
That run completed: `analysis/oracle/napa-native-render-finish-20261120.tsv`,
31186 contiguous human ticks, course4/level1/seed1, Drive5, finish/outcome3.
52 original scenes/pixel frames, every600render frames0..30600. Receipt confirms
restoration of the working `E:/RR/AWEMAN32.DLL`; original process21344 exited.
The exact original `E:/RR/ROADRASH.EXE` was used in place. No original capture
has been started since. Capture log:`build/napa-native-render-finish-capture.log`.
Trace SHA256:`d3552d2dc973d27b94fc8a0ad2b3057428a46df29e97e2ef9d79bf0d8338f1bb`.
Restored DLL SHA256:`ca7f2f65bf6d1550857556a27d65d4f44d7a6ca14c2be89b7c0f9affb6a47022`.

Other long rendering evidence:`hiway-native-render-finish-20261119.tsv`,20,710 ticks,
35 scenes. Short Napa:`napa-seeded-render-20261119.tsv`,240 ticks/7 scenes.
Existing Canyn/City/Medly rendering dumps are component windows, not complete
seeded human simulation recordings with composed scenes. The separate accepted
five-course finish recordings establish simulation/RNG coverage, not missing
render coverage. Commercial recordings and assets remain local/ignored.

## Fixes recovered from the retained Napa run

- Tick1035/frame1200: rider shadow differed after contact shoved the bike30world
  units sideways. `004067d0` had already copied the riding half. Riding projection,
  visibility, interpolation and height now read the retained half; crash bike/body
  poses remain independent. Synthetic view-state test and actor-placement test
  exercise the distinction.
- Tick8896/frame9000: eight STOP3E/G signal-head nodes missing. The earlier claim
  that `0041f220` was dead was incorrect: `0046dfb0` load descriptors indirectly
  populate `00468ee8/ef0/ef4/ef8` with GLOBAL CEL11..14. Import and draw housing+
  controller.signal0 phase overlay, index-bit2 mirror and construction-time ground.
  CCB75x159 becomes indexed127x158 after first projection even when rejected; retain
  the prepared hotspot for that head's overlay, normalized dimensions for later
  heads. Reverse creation order resolves equal-depth ties. Actual creation order
  is0,2,3,1 (earlier working notes incorrectly wrote0,2,1,3). A separate serial
  avoids changing car ordering/RNG. Signal height/half/serial are schema5 snapshot
  state, covered by a nonzero snapshot round-trip check.
- Tick17362/frame17400: last common slice before a fork used one forward branch
  for all cached props. `00439b10` follows side-specific links. Supply left/right
  physical neighbors and cache before continuity on both. A merge-window
  single-to-dual reveal is different: retain the continuing arm's neighbor. The
  first regression run exposed this at Highway tick16175/frame16200 (31vs30
  cached ground); restricting the two-link treatment to real split events fixes
  it. Synthetic ring callback test retains both sides despite choosing branch1.
- Tick21636/frame21600: last dual-to-common merge collapsed to the player's arm,
  making an inner hill an exterior viewport band. `004442f0..004443ae` retains
  both physical sides at event1; collapse applies to the other departing-window
  case. Scenery follows the same topology. Event4 divider uses the approaching
  roads' forward lane endpoints (+34/+30 at +28 row), with no split-edge padding
  (`0043bd22..0043bd45`). An initial retained-projection-history experiment was
  removed after tracing this special path; no guessed projection cache remains.
  Synthetic scene regression checks distinct forward Y and unpadded endpoints.

## Comparison tooling and scope

`--render-camera` checks native camera position, physical half, absolute X and
height, failing on disagreement. `--render-scene` uses native camera, road ring,
terrain, actor and scenery state; it imports none of those inputs from the scene.
It builds native draw lists between sparse observations so shared presentation
resource state advances normally. This is numeric work, not per-frame visual
analysis or per-frame pixel rendering. Geometry, textures, flips, painter order
and every remaining original node are required to match; no divergence whitelist.

`oracle-scene-test --discover-scenes` finds retained SIM scene/pixel pairs and
reads captured framebuffer descriptors directly. Explicit SIM mode converts
course enum1..5 to runtime0..4. The original-list raster test repaints the complete
original list over its captured framebuffer. It proves submitted-list raster
behavior, not independent background clearing or previous-frame retention.
Both long captures pass this test (Highway35, Napa52), zero pixel differences and
zero unmatched resources. New CMake tests register both raster sets and full Napa
composed comparison.

## Checks

Full Release build:`build/native-render-final-build.log`.
Focused gate:`build/native-render-final-focused.log`:42/42pass, no skips,129.21sec.
Strict five finished human races:`build/render-five-finishes.json`:5pass,0skips,
0known divergences,0unexpected failures,143764ticks including RNG; fixtures are retained in the C-worktree analysis/oracle folder,
assets`E:/RR`, binary`E:/road-rash/build/release/oracle-sim-test.exe`.
Direct extended Napa:`build/napa-native-merge-divider.log`:31,186 ticks,52 windows,
1,554 placements/heights,13343generated nodes match. Twelve audio-thread RPM timing
notes and one golden-only metadata field are explicitly outside its equality
claim; these are not simulation/RNG or scene divergences.

First focused gate failed Highway link choice, stale actor-placement fixture and
incorrect test-only signal-order expectation. All were investigated/corrected;
the final42/42gate is green. No fixture/golden was changed.

## Remaining evidence requiring future captures

No further original captures are allowed now. Full composed end-to-end coverage
for Canyn, City and Medly needs seeded human runs with matching render dumps and
finish receipts; existing isolated windows cannot prove those complete histories.
Higher levels, alternate fork choices/bikes, crash/remount, nitro and police
scenarios need targeted composed evidence beyond what current sparse windows
exercise. Independent complete-frame background retention/horizon/HUD and modern
interpolated presentation need frame-boundary evidence, rather than repainting
an original list over an already completed original framebuffer. Existing data
cannot establish every frame between the52 Napa/35 Highway samples. Do not claim
all race rendering is1:1 from these sparse measurements. Do not launch captures
without a later user instruction lifting the constraint.

Delivery: commit/push Windows, clean rr-game build, asset-free1.1.34 installer/
release (do not execute it), then report the capture-dependent coverage above.
All gates are complete. Keep source, docs and retained evidence.
