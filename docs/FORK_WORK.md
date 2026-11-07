# Windows fork work

Branch: `port/plan33b-windows` in `P0NYSLAYSTATION/road-rash`.
Base: arkits/road-rash `6b3b6be9ca5698b8b17f8f1ab53f2744a5202238`.
Local checkout: `E:/road-rash`. Local extracted audio: `E:/RoadRash3DOAudio`.

The requested work adds a selectable native 3DO audio backend, built-in
FluidSynth MIDI, a Direct3D 9 presentation wrapper, in-game asset/save browsers,
VS Code/MSVC/Ninja presets, extraction tooling, and an asset-importing installer.
Feature commits are separate; no commercial assets are checked in.

## Validation

Windows 11 x64, MSVC 19.44.35211, Ninja, CMake and LLVM 22 formatting.
Both Debug and Release executables build; Release retains libdatachannel
network transport using statically linked OpenSSL 3.6.5. FluidSynth 2.6.2 and
libsndfile 1.2.2 are replaceable DLLs from the checksum-pinned official archive.

The Release suite reports 102 passed, 36 skipped, zero failed (138 total).
The missing-disc/reference-trace tests explicitly skip; these results do not
claim original-hardware oracle parity. Additional runs with the supplied PC
assets pass the full frontend/save tests (including three save-list pages),
and render all five race MIDI songs plus Klown through the built-in synthesizer.
Mixer tests cover sustain endpoints, raw frequency, quiet amplitude, interpolation,
linear panning and integer angle quadrants. A real hidden Windows HWND test
checks Direct3D 9 present, resize/reset and absence of child windows.

The installer compiles with Inno Setup 7.1.1. Silent installs into isolated
test folders pass with and without optional 3DO audio. Each launches with its
automatically discovered imported assets and produces six seconds of nonzero
race audio. Imported sample bytes match the extraction output; the PC source
installation is read only. That initial release also imported `VIDEO BAK`; 1.1.0 removes that fallback. The installer stage uses a file allowlist, includes source
revision metadata and licenses, and rejects commercial/stale assets.

Local evidence (not committed): `build/release-verified-tests.log`,
`build/frontend-final.log`, `build/midi-final.log`, `build/install-smoke.json`,
`build/install-pc.log`, `build/install-3do.log`, and `build/extraction-final.log`.

## Fidelity and scope

[FORK_AUDIO.md](FORK_AUDIO.md) records source routines, exact sample conversion,
runtime laws, and the remaining procedural-DSP/PC-simulation boundaries.
No EQ, DC removal or loop repair was applied. Extra sampled banks and movies
are extracted in addition to the core race bank. Five EAMS scores are retained
as MIDI with optionally rendered, explicitly labelled GM WAV previews.

Original engine WAVs in `E:/RR` remain restored and separate from this project.
Release downloads contain the installer and checksums, not extracted audio.

## Windows 1.1.0

The DX9 wrapper previously captured only SDL's logical viewport and stretched it
across the backbuffer, discarding integer-scaling borders. It now captures the
full presentation. The 320x240 path has a native-size framebuffer and calculates
its own centered destination and mouse coordinates. Don't override preserves
original High/Low/Inset choices.

Windows no longer initializes SDL's controller subsystem. Native XInput polls
independent triggers/sticks; DirectInput enumerates legacy controllers, excludes
XInput duplicates, uses foreground/nonexclusive acquisition and reacquires after
focus loss. Hotplug is polled. Ten configurable actions include separate turn and
lean directions. Native control-table ordering and full-scancode persistence are
corrected. Non-Windows controller access retains SDL.

An embedded room server runs over framed nonblocking Winsock TCP. UDP broadcasts
and loopback discover LAN rooms; direct IPv4/IPv6/DNS invites allow Internet
connections with host port forwarding. UI connection work is asynchronous.
Build and gameplay-data digests still gate sessions, but optional audio/video/menu
images and patched original executable machine code do not. DATA files and the
original executable's static data sections identify gameplay tables. OKPlay 0.8.3
transports original game messages; its wire protocol is incompatible with this
port's race protocol. No relay service or automatic NAT traversal is claimed.

Setup imports an installed folder, original PC CD directory/drive, or ISO via
rr-import. The optional 3DO archive is unpacked and converted by bundled offline
helpers (Python/PyInstaller, full 7-Zip reader, minimal LGPL FFmpeg). No licensed
soundtrack is extracted. Existing banks are retained until conversion succeeds.
Only VIDEO is imported. The same public AppId enables updates; test installers
use a separate AppId and suppress shortcuts to protect the real installation.

Validation: Release CTest has 141 registered tests, zero failures (36 unavailable
fixtures skip); the real-disc frontend and native-screen tests run separately.
Native input tests check XInput/DirectInput axis/button/POV translation, separate
turn/lean bits, binding capture and exact settings round trips. A real hidden HWND
checks 640x480 integer borders, 320x240 framebuffer sizing and pointer mapping.
Real TCP sockets pass host/join, UDP discovery, chat, votes, race setup and sealed
race inputs. Physical controller presses and two-machine/WAN play remain untested.

Isolated setup tests use the old 1.0.0 installer script/runtime with a test-only
AppId, then update with no asset arguments. Saves, settings, soundfonts, renderer,
custom config and the imported bank survive; the new binary launches and renders
nonzero race audio. Fresh PC-ISO plus 3DO-ZIP installation extracts 220 WAVs and
launches using 3DO audio. PC-only folder import launches with PC audio and ignores
VIDEO BAK. Evidence: build/v11-installer-validation.json, frontend-real.log,
v11-tests.log and fresh-cd-archive.log. These contain local paths and are not
release assets. Converter corresponding source archives accompany the release.

Additional checks: BIN/CUE imported from a 7z archive produces the same 220 WAVs
as ZIP. A damaged archive returns setup exit 1 and preserves the current bank.
Repeated imports replace read-only destination CD files without changing source
attributes. Debug also builds and passes its 138 registered tests (36 fixture
skips). The optional upstream WebRTC test targets account for the Release count.
