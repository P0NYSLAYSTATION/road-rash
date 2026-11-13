# Actor resources, road motion and construction clocks — November 11, 2026

Baseline: 4807fc3, following the requested merge of upstream main 20fb4c0.
Work stays on codex/simulation-parity. E:/road-rash multiplayer edits are untouched.

## Recovered behavior and implementation

- Pedestrian graphics replacement (0044143b–00441474) clears the loaded word
  and animation descriptor while retaining old animation-bank presence words.
  Model eviction separately from the cached look. Resolve families from the
  actual front walker: CourseScene already includes the view-slice bias.
  City slot0 changes 10 to 12 at frame505/front165, reaching the MSRR load
  event137 with bias -28; pedestrian6 notices on think514. Replay now compares
  the pre-draw resource checkpoint rather than the previous frame's cache.
- Traffic uses the same weighted bend/rise/heading accumulator as other
  actors (00439270/004394c0). Load its atan table at 00476768 and preserve
  multi-sample segmentation. Expanded City body probes exposed the missing
  heading accumulation at tick204. Rider accumulation delegates to the same
  helper without changing its arithmetic.
- Capture the initial scheduler nodes before their first think. Both traffic
  controllers and pedestrians are constructed at tick0 in E:/RR, not tick1/2.
  First controller dt is31, so a 600-tick phase changes at571, not601.
  This supersedes the earlier inference from teardown/first-run cadence:
  that cadence alone cannot distinguish next0 and next1. Production defaults
  now use zero; replay retains explicit old-fixture overrides.
- RNG is anchored once before race setup, then free-runs. Compare actual
  native call counts against the installed original counter. New captures
  explicitly report rng.counter_enabled, including zero-draw opening ticks;
  old traces can establish interception through any buffered randlog entry.
  Original code-address randlogs are diagnostic attribution, not portable
  state. No per-tick reseeding/reanchor is used.
- First captured frame is POST tick1. Reproduce its countdown consumers
  before checking the initial RNG: City seed1 yields742760408 after three
  draws; Napa yields1133891140. Neither initial stream is silently repaired.
- Installer notices include the merged FreeType/libjpeg dependencies and
  the LGPL Wine fallback font's license and FontForge source.

## Validation and limits

Full Debug build passed. Normal-host CTest with RR_ASSETS=E:/RR passed all
enabled tests: 138 registered, 102 passed, 36 explicit corpus/platform skips,
zero failures (build/parity-lifetimes-host-tests-console.log). Bike weighted
motion, signal construction-clock and pedestrian eviction/reload regressions
pass; pedestrian/traffic corpus cases use the actual E:/RR assets. A sandbox
test attempt failed on temporary-directory permissions and was stopped;
the normal-host run above is the usable gate.

Full Release build and CTest also pass: 141 registered, 105 passed, 36 explicit
skips, zero failures (build/parity-lifetimes-release-tests-console.log).

Measured intermediate strict checks: City600 PASS in
build/initial-clock-city.json; expanded Napa2400 PASS in
build/merged-counted-napa.json (before the final construction-clock change).
These historical receipts do not certify broader/final coverage.

Final current-source strict acceptance also PASS: both old expanded Napa2400
and City600 fixtures, plus the new explicit-counter City4 trace, with the
Release binary (build/lifetimes-release-accepted.json). Debug confirms the
Napa2400/City600 pair (build/lifetimes-final-accepted.json). This preserves the
known scope while retaining the fresh, different-trajectory failures below.

Fresh complete human captures, all from E:/RR/ROADRASH.EXE, seed1:

- napa-clock-motion-2400.tsv SHA256
  b1ee857018ed76a69894f48b3e2167dffd5729d79249b800d27dc5e672a51b63:
  first expanded divergence tick1072, rider13 overtaking a car. Native equal
  scan keys put a far-lateral bike before the car and terminate the lateral
  window scan; original latches car3189248. Persistent mixed-node ordering
  needs tracing, not a guessed blanket car priority. Evidence:
  build/napa-clock-motion-all.json, build/napa-scan1072.log.
- city-clock-motion-2400.tsv SHA256
  455cfeb732a1524b01cde1ebee888278255eb72cd1fad7cd2298eb2ec438a326:
  first expanded divergence tick751, crossing-car lateral position differs
  by800. Evidence build/city-clock-motion-2400.json.
- city-clock-counter-4.tsv confirms rng.counter_enabled=1,
  traffic.setup_tick=0 and pedestrian.setup_tick=0.

No claim of complete parity or all-course/race acceptance. Next independent
parity slices are the two first divergences above and the upstream documented
draw/topology/other-course gaps. Multiplayer remains on hold. Commercial
traces stay local. The working AWEMAN32.DLL was restored and SHA256 verified
ca7f2f65bf6d1550857556a27d65d4f44d7a6ca14c2be89b7c0f9affb6a47022.
No reference executable/DLL installation is involved.

## Published progress release

Source commit15b46a106585e968a90ef73a7906af2b8a597fa7 is clean and pushed to
origin/codex/simulation-parity. Release3do-1.1.2:
https://github.com/P0NYSLAYSTATION/road-rash/releases/tag/3do-1.1.2
Installer SHA25690a68c83730692bc9e1117b5538e0d0e946b076df6ec554ba0b46d3056a44c6a.
Public download checksum verified (build/release-112-publish.log).

Isolated test-AppId update1.1.1 to1.1.2 without asset arguments preserved all600
checked assets, saves, soundfonts and settings; no changed files. Installed
BUILD_INFO identifies the clean source above. Installed main menu rendered
and was visually inspected (build/parity-installed-main.bmp, local commercial
art only). Receipts: build/parity-upgrade-validation.json and upgrade logs.
Production AppId/shortcuts were untouched by the isolated upgrade test.

No original process, capture or build remains held. The C tree has no shared
resource locks. Resume at Napa1072 mixed-node scan order or City751 crossing
motion; keep the new failing captures and established passing cases separate.
