# Windows frame pacing

Controller discovery runs once at initial window setup. XInput slots are
probed once, and DirectInput/raw-device enumeration runs once. The game polls
only those startup controllers during play. Connect controllers before launching;
restart the game to discover a controller connected later. Window recreation
rebinds existing DirectInput devices without enumerating again.

The previous implementation enumerated devices on the rendering thread every
two seconds. A local 600-frame menu trace measured two scans at 75–172 ms;
DX9 Present stayed below 7 ms. An intermediate background-discovery prototype
was removed at the owner's request. The final startup-only implementation's
2,400-frame menu trace recorded one 179.214 ms startup scan, controller polling
at most 0.045 ms, and presentation at most 16.786 ms. No recurring discovery
stalls occurred in that measured run. This measures the installed machine;
physical controller interaction and other hardware still need device validation.

Set RR_RUNTIME_TIMINGS to a writable CSV path to record start/duration nanoseconds
for controller polling/discovery and presentation. It does not modify simulation.
Local evidence: build/menu-dx9-timings.csv (before),
build/menu-dx9-startup-only-timings.csv (after). Commands:

```powershell
$env:RR_RUNTIME_TIMINGS = 'E:/road-rash/build/menu-dx9-startup-only-timings.csv'
$env:RR_RENDERER = 'dx9'
python -c "import subprocess; raise SystemExit(subprocess.run(['build/release/rr-game.exe', '--assets', 'E:/RR', '--windowed', '--menu-frames', '2400'], timeout=90).returncode)"
build/release/modern-port-test.exe controllers
build/release/modern-port-test.exe scaling
```

Both controller mappings/binding persistence and the integer-scaling framebuffer
regression checks pass. Release compilation succeeds. This frame-pacing fix does
not establish completion of the separate multiplayer or simulation-parity work.
