# Mini dashboard wiring — 2026-11-13

The live race canvas always passed DashMode::full to the existing HUD renderer,
and no F10 event mapped to a race action. Thus mini_dash loaded/persisted but was
never used when composing the race picture. The existing helper-only cycle
checks did not cover that missing connection.

The race screen now takes mini_dash from its live settings and composes Full,
Small or Off accordingly. It also selects the existing low-resolution HUD art
for Low/Inset race modes. F10 key release cycles 0 -> 1 -> 2 -> 0 and saves to
the active settings path. Key-down does not double-cycle or dismiss the original
pause/quit message. F10 remains available with the pause menu open.

The in-game pause menu has Dashboard / Full, Small, Off. The choice updates and
persists the same mini_dash setting; no separate competing setting is added.
Countdown and pause/quit text remain independent of the dashboard mode.

Focused evidence: build/mini-hud-tests.log has passing race-actions, HUD, frontend,
settings and native_screens. Its initial screen-render assertion used an all-zero
fixture palette; the fixture now initializes DATA/PALETTE.RAW as game_main does.
The final build/mini-hud-screen-test.log passes the real RaceScreen event and
render path: a loaded mini_dash=1 produces the small picture; F10 key-up moves
through all three visibly different frames and back to the original small frame,
with zero gameplay ticks advanced. Pause menu selection and saved mode are also
checked through actual menu activation. No original captures or installer
upgrade runs were performed for this change; excluded menu-pixel goldens were
not pursued.
