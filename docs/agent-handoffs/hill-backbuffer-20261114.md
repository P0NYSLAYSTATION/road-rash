# Hill backbuffer preservation — 2026-11-14

Baseline: Windows branch `port/plan33b-windows`, `5b975c8`.

## Diagnosis and original evidence

The hill report reproduces in headless scene rendering. HIWAY sample 3477,
fraction 255, camera (0,150), native 640x480 has 26,528 genuinely unwritten
pixels below row 320. CITY sample 705:255 also exposes the lower background.
Two renders with different initial values distinguish unwritten pixels from
valid opaque palette index zero. The Hiway preview has a broad black lower band.

Read-only disassembly used the working `E:/RR/ROADRASH.EXE`, not another install.
`00443d70` initializes `y = -((fraction * rise) >> 8)` and depth
`256-fraction`; `004446c9..004446d7` projects the row before
`00444c37..00444c54` advances depth/x/y. Native projection already follows this
order. The near depth table is clamped at 163840, so not every downhill pose
covers the bottom of the screen.

`004439f0` builds the scene, submits the draw list (`00450420`), then presents
(`0044c780`). Within scene construction, `00443a80` calls `0044c800` to blit
horizon rectangles to the existing DirectDraw backbuffer. The source height is
297/320 high-mode rows or 160 low-mode rows (`00448a90..00448ade`), rather than
the entire race view. This path does not clear the lower backbuffer. The port's
`CourseScene::draw_horizon` instead cleared the whole framebuffer before every
horizon draw, turning temporarily unwritten lower pixels black.

## Change

Remove that whole-frame clear. CourseCanvas retains a clean world framebuffer
in indexed and RGB paths, then copies it for HUD/post-processing/presentation.
Consequently retained pixels cannot contain stale dashboard art, palette fades,
or repeatedly applied grading/bloom. The RGB horizon conversion updates only
its actual 320 scaled rows; fog colour reads the bottom of those horizon rows,
not the retained terrain. Buffers are recreated on resolution changes.

No road vertices, depth-table values, texture UVs or simulation fields changed.
This addresses the reproduced flicker through backbuffer preservation. It does
not establish complete original pixel parity or quantify the separate impression
of reduced texture warping. The native history uses the last clean world frame;
original DirectDraw flip-buffer age was not measured in a live capture.

## Focused evidence

- MSVC Release `course_scene-test` and `race_screen-test` build successfully.
- `RR_ASSETS=E:/RR course_scene-test --crest-regression`: 769 adjacent Q8 poses
  per descent, 1,538 total. Both differently initialized buffers agree below the
  horizon after the starting-grid seed. HIWAY preserves 1,603,208 pixel writes
  that the old clearing path turned black; CITY preserves 60,280.
- `RR_ASSETS=E:/RR SDL_VIDEODRIVER=dummy race_screen-test --crest-canvas`:
  actual CourseCanvas retains 23,872 exposed lower pixels in each indexed/RGB
  and Full/Small/Off HUD case. Tests contaminate presentation buffers separately
  to catch retaining HUD pixels, and check Off output contains the clean world.
- Release CTest `^(race-screen|race-crest-canvas|course_scene)$`: three passed,
  zero failures/skips. Logs: `build/crest-fix-build.log`,
  `build/crest-canvas-build.log`, `build/crest-regression.log`,
  `build/crest-canvas.log`, `build/crest-focused-tests.log`.

The previous mountain coverage loop was invalid: draw_horizon cleared both
comparison frames to zero before rasterization, so it could never identify
unwritten lower pixels. Its claim of zero gaps is superseded by the dual-clear
measurement above. The replacement tests the real adjacent-frame behavior
rather than assuming every independent static pose paints every lower pixel.
This is renderer/component evidence, not a new original-game capture or a
simulation replay. No desktop replay or installer upgrade was run.
