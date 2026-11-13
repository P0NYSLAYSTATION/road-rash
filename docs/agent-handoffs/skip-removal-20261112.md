# Remove every skipped release check — active continuation

User explicitly requires every skipped/excluded check to be fixed, alongside the
standing autonomous parity task. Do not stop at a status checkpoint or publish
another release with skips/exclusions. No questions/permission needed: builds,
commits, pushes and releases are already authorized. No subagents authorized.
Current baseline remains720b591, branchcodex/simulation-parity, public1.1.18.
No commit or release after1.1.18 yet; all current edits are in flight.

Original runtime remains ONLY E:/RR/ROADRASH.EXE with its working AWEMAN32.DLL
instrumented additively and restored by tools/oracle_capture.py. No original
capture is active now; the last raster capture completed and restored its DLL.
No native GUI tests during an original capture. Preserve E:/road-rash multiplayer
WIP and dependency junctions. No reference installation or alternate original EXE.

## Verified parity progress before the skip audit

See parity-higher-levels-20261112.md for full history. Original conservative
Hiway L5 ended in arrest94929, not a finish. Extended pedestrian motion fixture
revealed physical-ring ground reads; Race::vertical now follows retained current
and next slots, with focused retained-ground test. FieldReplay now compares real
original slice pointers normalized by fixed ring0049f250/stride2f8/offset30 or194,
instead of incorrectly assuming position>>8 equals the original retained slice.

The next real long divergence58545 was missing00454890 split hillside adjustment:
positive countdown zeros the inner three hill heights; nonpositive countdown
joins the two inner polylines through00454c30. Implemented adjust_split_terrain in
terrain_ground.cpp, integrated after both TrackRing halves, retained optional
side-height overrides and normalized-data flag in TerrainSlice. Important: avoid
no-op post-slice callbacks (fixed a track_ring regression). The strict expanded
2400 prefix AND all94929 arrested ticks now pass, one RNG anchor, zero allowances:
build/hiway-l5-split-terrain-acceptance.json. Synthetic ground fixture2112 original
samples passes. Full split-join component fixture comparison is still to add/run;
the original disassembly files are build/split-shoulder-disassembly.txt and
build/split-hill-adjustment-disassembly.txt (00454890), plus00454c30 midpoint.

Input-only mode5 preserves mode4 and adds forward lane, proper16.16 bend threshold
and normal kick near cops. Prefix6000 exact; full mode5 ended arrest13813, not
finish (SHA6dc041139cff6fe9efff8d1f0eee6a363ae9049439540555da0dd9fd4de8ee04).
Do not launch another blind long attempt: driver still needs traffic/police
avoidance. No finished Level5 recording yet. Five Level1 full finishes remain
the released milestone; rerun all five after current code integration.

## Skip audit and concrete fixes

Original release gate:104 passed,35 skipped,2 D3D exclusions. Raw audit is
build/release-skipped-check-audit.json. Added tests/test_assets.hpp and changed49
test sources to use RR_ASSETS/configured root. CMake RR_TEST_ASSETS_DIR cache is
E:/RR; all CTests inherit it, game wiring and race MIDI commands use it. C patch
preflight honors it too. Python replay honors RR_ASSETS. No original asset files
were moved. Build/configured-assets-full-build.log passes.

Deployment tests previously returned77 on Windows solely because module-level
fcntl import was Linux-only. Moved fcntl into the actual Linux CLI main; removed
the Windows suite early exit. All10 real synthetic deployment contract tests
pass (build/windows-deployment-contract-tests.log).

Both previously excluded Direct3D checks now run and pass after original capture
closed: build/reinstated-direct3d-tests.log. Keep both in the gate. Do not claim
an unmeasured cause for the earlier88760868 failure.

Restored headless corpus suite executes patch, race_setup, PE strings, career,
WAV, race_multi, rollback, net_client, deployment, built-in race MIDI successfully.
net_client legitimately takes71sec (84 multi-client scenarios), not a hang.

Full no-exclusion audit build/unskipped-full-gate-audit.log, build/release/build-audit.xml:
141 tests,117 passed,15 skipped,9 failures. Missing checks: oracle-sim, field,
schema2, L5, oracle-road, oracle-scene, terrain Canyn, terrain Medly, raster,
hires-render, raster-rgb, widescreen, polygon, text, seeded save. Failures exposed:
race-cache, race-screen, AVI, Cinepak, frontend, music, track_ring, race-soak,
net_frontend. Also internal optional picture/glyph/menu fixture checks must be
restored; do not hide them behind a superficially zero top-level skip count.

Soak failure14792 rider5 jumped339960->333952 without crash. Trace proves a
virtual-to-live wake: build/soak-wake-transition-trace.log, original wake placement
law004060b0. Replaced false continuous-motion assertion across this transition
with explicit wake-state checks (live, slice present, fraction128, +134/+138 equal
position). Kept continuous-motion bounds for other states and added exception
diagnostics. Soak now PASSES30.59sec in restored-fixture-media-regressions.log.
No simulation law was changed just to satisfy it.

## Media and stale MIDI assertions

E:/RR intentionally has no VIDEO directory. Do not enable VIDEO BAK. Extracted
only59 VIDEO files from supplied legitimate PC ISO into
artifacts/pc-test-media/ROADRASH/VIDEO (310MB), logpc-test-media-extraction.log.
No executable/disc reference copy extracted. Added explicit optional video_root
to Assets; defaults preserve ordinary lookup, no backup-directory search. Tests
AVI/Cinepak use rr::test::media_root; frontend/native_screens/race_screen/net_frontend
pass explicit media root while primary assets remainE:/RR. CMake
RR_TEST_MEDIA_ASSETS=absolute workspace artifacts/pc-test-media/ROADRASH. Assets
regression added proving ordinary lookup ignores VIDEO BAK and explicit CD videos
do not affect primary DATA lookup; needs focused rebuild/run after latest edit.

AVI and Cinepak now pass. Movie-related frontend failures disappear with actual
media. Music's stale SF1-player assertions now check the requested built-in MIDI
backend (GRUNGE4 and its actual parsed event count); legacy SF1 parser/render
checks remain. Music passes. Frontend still fails keyboard/joystick default
restoration: inspect modern controller/keyboard defaults versus stale expectations
around tests/frontend_test.cpp. Do not disable those checks.

Latest nine-test focused run build/restored-fixture-media-regressions.log:
polygon,AVI,Cinepak,music,track_ring,soak pass; raster failed missing scenario-tag
raw filenames (fixed publishing below; rerun), frontend only two binding defaults
fail, net_frontend crashes with unhandled exception (needs diagnostic catch and
actual fix). No active CTest session; exec78402 completed, may need final polling.

## Native component capture repairs and current raster fixture

RR_ORACLE_RASTER=2 originally completed its entire fixture/end marker but left the
original at its menu forever; the coordinator correctly rejected timeout124.
Added deferred exit thread after completed component-only capture. A thread
started from DllMain waits for loader initialization to finish, then exits0.
Ground-only mode uses the same safe termination. Helper
oracle/build-windows-component-exit/AWEMAN32.DLL builds and runs in E:/RR.

tests/golden.hpp original_executable accepts the two verified stock/working hashes
(working change is only startup compatibility). Updated metadata checks across
oracle raster/road/scene/polygon/terrain/etc and CEL; no trace hashes rewritten.

Native coordinator renamed raw companions to user scenario while traces still
referenced immutable capture-tag names. Fixed tools/oracle_windows.py to publish
both original-tag raw files and canonical aliases. Recaptured raster-fixtures
successfully: build/raster-fixtures-complete-sidecars-capture.log, trace SHA
17cb613409640093994d0393c2258626dfff84fcd788c4b26a6afeb6964a0325,
scenario capture-7c523022e42e470e85ba6170e00213b8. Immutable bundle + flat raw
sidecars now preserve referenced names. Rerun oracle-raster; polygon already passes
on the prior capture. Do not bless incomplete fixtures. Add a publishing regression.

## Missing glyph evidence — next work

Native Windows GDI probes compile with existing MinGW (no SDK install): prepend
C:/msys64/mingw32/bin to PATH, gcc -O2 tools/{glyphprobe,textprobe,blendprobe}.c
-o build/native-{name}.exe -lgdi32. Initial direct GCC attempt lacked its runtime
PATH and did not produce probes; corrected invocation succeeds.

Successfully generated genuine current Windows GDI evidence under
analysis/frontend/glyphs-windows-20261112/:
mono.txt,gray.txt,metrics.txt,sysmono.txt,sysgray.txt,blend.txt.
Commands glyphprobe.exe E:/RR/TEXT/ mono|gray [sys], textprobe.exe E:/RR/TEXT/,
blendprobe.exe E:/RR/TEXT/ (default4096 backgrounds). Output files endDONE.
Current text-test hardcodes missing analysis/frontend/glyphs-20261031 and macOS
system font paths; add explicit RR_TEST_TEXT_PROBES configuration and use Windows
Fonts/wingding.ttf,courbd.ttf for native system-face comparison. Keep all glyph,
advance and16-bit blend comparisons; do not remove failing cases. Native GDI
gray may differ from Wine-FreeType assumptions; measure before changing renderer.
TextRenderer currently emulates Wine-GDI via pinned FreeType2.13.3 (src/text.cpp),
so a Windows GDI backend may be needed if actual probes expose differences.
Probes AddFontResource; add matching RemoveFontResource cleanup for future runs.

## Remaining gate work and process state

No original capture active, latest AWEMAN restoration receipted. No native replay
active. Source has about80 modified files; preserve edits and explicit staging.
Full build after Assets media API passed build/restored-media-and-fixture-build.log.
Current Assets-test source got a regression after that build and needs rebuild.
Next actionable tasks:

1. Re-run raster fixture check with retained names; configure text probe paths and
   execute real text comparisons; seed original save fixture using capture_save
   but change its hardcoded C:/save-seed.RRS to capture-owned scratch path and emit
   valid standard frame/byte fields or a separately receipted binary fixture.
2. Adapt frontend_acceptance.py native Windows path to exactE:/RR working DLL,
   preserving script captures/cache/bitmap evidence; existing tool is Wine-only
   and references a forbidden old install. Do not run that old path. Native
   oracle/frontend_probe.h is already embedded and script-driven.
3. Generate missing moving/scene/terrain fixtures from E:/RR with current helper.
   CMake/tests default names: napa-moving.tsv (hires/rgb/wide), city-scene.tsv,
   pch-tunnel-20261016.tsv, medly-jump.tsv, napa-attract.tsv, four old sim names.
   Prefer explicit current fixture configuration/descriptive scenario provenance;
   do not rename modern captures to impersonate older scenarios or edit goldens.
4. Fix race-screen finish fixture: set_finish_q8 no longer overrides finish_course;
   inspect intended setter semantics rather than altering normal race finish laws.
   Its arrest fixture and net_frontend also need real debugging. Add catches to
   net_frontend to expose the exception before making behavior changes.
5. Race-cache still pins native frame hashes from20261017, before many measured
   simulation fixes. Do not blindly update constants. Separate fixed scene input
   for cache equivalence from live simulation, or establish replacement against
   fresh original evidence; keep threaded/HUD/no-mutation/allocation checks.
6. Full gate with every check enabled, zero skips/exclusions/failures, five full
   L1 strict races plus new expanded/arrested diagnostics. Commit/push explicit
   paths only, clean1.1.19 installer, update18->19 preserving600 files, public
   release/checksum. Outside19 scripts exist but are not run; their six-finished
   race assertion must match actual achieved scope (no L5 finish yet).

Continue autonomously until all requested work is done. No permission questions,
no checkpoint final reply, no stale approval-review rejection reused as a blocker.


## Continuation: user pause of all game replays/captures

The owner now needs the PC and explicitly prohibits any further game replay or
replay capture until their next message. No original game, recorded simulation
replay, GUI gate or live DLL instrumentation has run after that instruction.
Only builds, offline file analysis, memory-DC font probes/tests and synthetic
coordinator contracts have run. Keep this pause across context changes. Do not
launch E:/RR or any replay binary until a subsequent user message permits it.

The save-writer capture below happened BEFORE that pause. No original capture is
active; its working AWEMAN restoration is receipted. Compilation of a helper does
not authorize installing/running it during the pause.

### Newly repaired Windows components

- Native text now uses Windows GDI: monochrome DIB glyphs for race text and
  direct RGB565 DIB blending for menus. Font/DC/bitmap resources are retained,
  released by RAII and never create a window. Disc font registration matches
  original file registration. Installed system fonts retain normal linking.
- Actual Windows metrics differed substantially from the old Wine renderer.
  Native memory comparisons now cover ALL bytes, including previously excluded
  unmapped CP1252 bytes: 9,856 mono disc glyphs/advances, 9,856 RGB565 disc glyphs/
  advances, 672 mono system glyphs/advances, 672 RGB565 system glyphs/advances,
  and 196,608 coloured/background blend samples. All pass in
  build/native-all-byte-font-gate.log. Corresponding stdout counts are preserved
  in build/release/Testing/Temporary/LastTest.log until the next gate.
- Windows ANSI extent caching can return a mono fallback advance on RGB565;
  Courier New Bold DEL returned5 while the original probe and both Unicode
  extent and GetCharWidth32A returned10 (build/native-font-width-api.log).
  Fixed-pitch colour text uses explicit CP1252->Unicode extent measurement.
  Proportional/symbol and indexed text retain ANSI behavior; all byte comparisons
  above remain mandatory. No glyph or advance exception was introduced.
- Added text-system-mono and text-system-rgb565 CTests so probe surface formats
  have separate process lifetimes. tools/glyphprobe.c gray565 records full RGB
  expanded channels, not a 32-bit ClearType blue-channel approximation.
- Added checks for 292 original line-height sweeps, seven whole-string extents
  and six DrawText calculated rectangles from existing metrics.txt. Newly added
  layout checks are not yet run; the current full build is in progress.
- Frontend functional failures were stale removed-controller defaults. Tests now
  require modern keyboard defaults and reset controller_bindings. Net frontend
  uses the SAME gameplay_data_digest as the Windows game, accepting compatible
  installed art and startup patches. Assets/frontend/net_frontend passed after
  the native text change (build/native-memory-component-gate.log; text then had
  one advance discrepancy, subsequently resolved as above).
- Configured RR_TEST_GROUND_FIXTURE now loads the actual2112-sample original
  ground fixture; explicit missing input fails. Windows deployment contracts
  run instead of exiting77; the stale posix/skip property was removed.
- build/no-replay-component-gate.log passes6 memory/file checks: assets,
  terrain_ground, text, frontend, net_frontend, net_deploy. Its regex omitted the
  two new text-system tests; their separate three-test font gate passes. Rerun a
  corrected eight-test component regex after the current build.

### Capture tooling and corpus configuration (not live-run during pause)

Original save writer now writes capture-owned temporary output instead of C:/;
standard trace frame contains160 numeric save.byte.N fields. Component exits
cleanly after completion. Actual capture analysis/oracle/save-seed-native.tsv
completed/restored before the pause (trace SHA7e2d8d2abaa3b892cc81ca45cd7bf24953fe5c8586a4d27dcc3fc9b1bcb2f3a1).
The original RRS companion is analysis/oracle/save-seed-native.tsv-save.rrs.
Do not confuse this seeded writer fixture with a naturally driven career save:
that second fixture is still missing, and Windows career-save-corpus now FAILS
when it is absent rather than silently accepting it.

Native frontend tool now dispatches to exactE:/RR, runtime-profile and working-DLL
instrumentation/registry contexts, writes only owned scratch artifacts/saves,
validates completion/screens/companions, and publishes a receipt only after DLL
restoration. oracle/frontend_probe.h supports owned output directory, validates
labels and preserves spaces in save-dialog paths. Helper builds successfully:
oracle/build-windows-native-frontend/AWEMAN32.DLL,
build/native-frontend-helper-build.log. It has NOT been installed or run.

RR_TEST_FRONTEND_REFERENCE selects a completed new native capture run for menu,
picture-cache and driven-save comparisons, replacing hardcoded old run IDs.
Configured missing images/caches fail instead of silently skipping. Added
frontend-original CTest with RR_REQUIRE_FRONTEND_REFERENCE=1: without that
configured capture it FAILS explicitly. Functional frontend remains a separate
component check. No menu/picture pixel acceptance is claimed yet.

Four Windows simulation CTests now configure existing receipted human2400-tick
captures (Napa basic, Napa expanded, City clock-motion schema2, Hiway L5 expanded)
and strict acceptance/human driver/600-second timeout. Other platforms retain
historical diagnostics. Cache variables RR_TEST_SIM_BASELINE/FIELD/SCHEMA2/LEVEL5
allow explicit capture selection. These newly configured CTests are NOT run
under the pause. No new full-race acceptance is claimed.

Raw companions retain both capture-tag embedded filenames and canonical aliases;
RRS companions are retained, and published receipts include artifact_sha256.
Three synthetic coordinator tests pass in build/native-coordinator-contract-tests.log:
Windows failed/partial runs preserve accepted evidence and restore mocked DLL
state, success publishes immutable evidence, sidecar aliases/save bytes survive.
All process/DLL/registry boundaries are mocked: no game is launched. The two old
Windows-skipped tooling tests now test this native coordinator contract instead.

### Current next work

Full build build/offline-parity-repairs-build.log is running (exec session99128
at time of writing). Helper build is complete. After build, run memory-only
font/layout + menus/assets/deploy/ground checks, synthetic tooling suite, strict
missing-reference negative check, syntax/doc/diff checks. Do not run race-screen,
race-cache, race-soak, any oracle simulation, GUI or original game during pause.
Inspect/fix actual layout mismatches from existing metrics before new captures.

Remaining actual capture-dependent work: seven missing road/scene/terrain/moving
render fixtures, native frontend image/cache/driven-career evidence, race-cache
stale frame-hash investigation against a fresh original frame, parked-cop screen
fixture diagnosis against original, new higher-level finished race/branch coverage,
then full unexcluded native gate and strict five completed race regressions.
No commit/push/release has occurred in this continuation; HEAD remains720b591,
release1.1.18. Do not publish another incomplete release or claim zero skips from
focused component checks. No live capture/DLL ownership is held.


## Final offline verification in this continuation

All build sessions have completed. Latest main build:
`build/offline-final-logging-build.log` (exit0). Latest32-bit helper:
`build/native-frontend-helper-final-build.log` (exit0); not installed or run.
No original game or oracle-sim process is active. No live DLL claim is held.

Word-wrap diagnosis actually resumed after the one real review usage-limit error;
that error is NOT a current blocker. `build/native-layout-diagnostic.log` measured
original `(274,64)` versus port `(268,64)` for desc32-4. Windows draw_box now uses
native DrawTextEx for measurement and rendering. A shared RGB565 compositor also
preserves transparent HUD pixels and opaque black shadow ink; two regressions
cover direct text and wrapped text. All glyph/blend/layout/transparency checks
pass (`build/native-layout-and-transparency-gate.log`).

`build/offline-components-verified.log` and
`build/release/offline-components.xml`:10/10 focused tests pass, zero skips or
failures,11.57s. Includes text + two system-font tests, assets, terrain_ground,
frontend, net_frontend, net_deploy, agent-tooling and portability-lint. The tooling
suite itself passes27 tests with zero skips (`build/offline-tooling-full-gate.log`).
These are NOT the full145-test native gate or simulation replay acceptance.

Negative check `build/missing-original-evidence-negative-check.log` returns8 as
expected: frontend-original requires a completed configured capture, and the
career-save corpus requires an original-driven save. These remain genuine unmet
acceptance items. Seeded writer evidence separately proves all160 bytes match
original writer buffer and actual RRS (SHA45c601fd78b074b440b12188e3353a73f3b4c9ff9f2338c7853a5463619c388a),
`build/seeded-save-byte-evidence.json`; this original capture occurred before pause.

Additional coverage repairs: native-actors is now a registered CTest, rejects
missing v4/actor-hook frames and zero compared frames/nodes. Medly terrain gate
no longer passes --nodes-only: original rider grounds are required. Capture cache
paths RR_TEST_ROAD_FIXTURE,RR_TEST_SCENE_FIXTURE,RR_TEST_CANYN_TERRAIN_FIXTURE,
RR_TEST_MEDLY_TERRAIN_FIXTURE,RR_TEST_MOVING_FIXTURE allow fresh native provenance.
Asset-free CI keeps historical fixture behavior; this does not waive the owner's
asset-backed local acceptance. Missing required local frontend/save evidence fails.

`oracle/scripts/frontend-native-basics.script` is prepared (main menu, Cydney,
street, restroom, jukebox, controls, graphics); not executed. Native frontend
output defaults to analysis/frontend/acceptance-native. New native menu references
must account for the user's intentional controller/settings changes rather than
misrepresenting old-menu pixel exceptions as gameplay parity. Original image/cache
and unchanged-region comparisons still require actual new evidence.

Offline inspection of the prior mode5 Level5 arrested recording: final13813,
controls69, player condition195, crash1, speed119, outcome4. It was not a finished
race or bike-condition wreck. No new driver or simulation change was made from
this isolated observation. Mode4 and all accepted input streams remain intact.

Docs PROGRESS,VERIFICATION,REVERSE_ENGINEERING and generated CURRENT_STATUS now
record these facts. Syntax, diff whitespace and doc checks pass. No commit/push,
installer rebuild or release was performed. HEAD720b591 / release1.1.18 remains.

The PC-use pause remains respected, as stated in commentary after the owner's
latest generic continue. No game or recorded simulation replay was launched.
The remaining meaningful parity progress now requires original captures / replay
validation: moving/scene/terrain + native actors, original frontend/cache and
naturally driven save, fresh race-cache frame evidence, original parked-cop
comparison, remaining higher-level finish/branch coverage, then all unexcluded
native checks and five whole-race regressions. Resume those when captures are
permitted; do not restart the skipped-test audit or report the old review error.
