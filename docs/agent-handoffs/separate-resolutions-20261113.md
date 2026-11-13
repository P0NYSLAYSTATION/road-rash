# Separate menu and race resolution — 2026-11-13

The shared Render Resolution control and both Widescreen controls are removed.
The original Graphics Options / Race Resolution popup now offers Low (320x240),
Inset, 640x480, 1280x960, 1920x1440, 2560x1920 and Match display. The latter picks
the largest supported 4:3 size fitting the window/display. Menu Resolution is
independent: 640x480 (default), 1280x960, 1920x1440, 2560x1920, or Match display.
The original race default remains low-resolution. Changing either control does
not change the other. Pause menus use Menu Resolution and resume returns to the
race framebuffer. Original inset centers a 320x240 picture inside a 640x480 area.

Settings persist race_resolution and menu_resolution separately. Legacy
render_scale migrates to the race control only. Old widescreen/enh_widescreen
values are ignored and no longer saved; all live menu/race views use 4:3.

Composition now uses real offscreen SDL target textures at the selected size,
including sizes above the output window. Logical composition and final output
scaling are independent. Integer/Fit/Stretch apply once when the framebuffer is
presented by either SDL or DirectX 9. The race canvas no longer overwrites the
selected mode with Letterbox. Mouse mapping uses the same final destination.
Nearest/smooth filtering and supersampling follow the display settings.

Focused checks: settings, display, native_screens, frontend, integer-framebuffer
and race-screen PASS 6/6 (build/separate-resolution-final-tests.log). The explicit
SDL counterpart also passes (separate-resolution-sdl-test.log); the DirectX
rerun is separate-resolution-dx9-test.log. Both exercise 320x240, inset and
640x480 through 2560x1920 native targets, output state restoration and mouse
mapping. Frontend checks exercise all seven race choices and independent menu
changes. Existing menu-pixel goldens remain outside the owner's scope.

No original replay captures or installer upgrade runs were used for this change.
