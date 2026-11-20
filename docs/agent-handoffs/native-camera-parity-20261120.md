# Native camera and extended race rendering - November 20, 2026

Checkout E:/road-rash, branch port/plan33b-windows, baseline936dff8.
Runtime milestone committed/pushed/released1.1.33. Receipt
build/windows-v1133-published.json; installer not executed. All37 focused tests
and five finished simulation recordings143764ticks pass for that milestone.

Current test-only work removes the last imported scene input: viewport.
--render-camera audits native position/absolute X/height/half across captured
windows, returns1 on any mismatch.35/35 Highway viewports match.
--render-scene now projects with race.camera_position/camera_half/camera and
native road offsets. Both Napa/Highway composed tests still pass without any
borrowed world/actor/scenery/terrain/camera state. Logs:
build/hiway-native-camera-audit.log, native-view-composed-gate.log/build.log.
This remains sparse simulation-tick evidence, not every frame/pixel or modern
presentation interpolation.

## Active original capture

Scenario napa-native-render-finish-20261120; course4/level1/seed1/human/Drive5.
Original exact E:/RR/ROADRASH.EXE PID21344, started11:20:56 local November20.
Tool session76754. Wrapper instruments working AWEMAN32.DLL in place, backs it
up and restores it on exit. Do not touch/rebuild that active DLL/helper.
No reference installation or copy elsewhere. No installer execution.
SIM/RANDCOUNT/FULL_SCENE/GROUPS/PIXELS1; scene every600render frames;
UNTIL_FINISH1,max45000ticks,timeout850sec. Drive5 has five-second stalled-rider
recovery. Log build/napa-native-render-finish-capture.log. Only publish_capture
creates canonical output after validating header/contiguous ticks/completion.
No claim of finish until receipt confirms race_finished/outcome.

Next:monitor bounded progress from the temporary capture and then compare
analysis/oracle/napa-native-render-finish-20261120.tsv with --render-scene.
Resolve the first numerical mismatch, not visual inspection of every frame.
Preserve accepted evidence; no commercial files committed.
