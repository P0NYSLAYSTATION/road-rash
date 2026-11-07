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
installation is read only. Cinematic fallback and the user's `VIDEO BAK` layout
are supported. The installer stage uses a file allowlist, includes source
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
