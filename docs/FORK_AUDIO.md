# Windows 3DO audio adaptation

The Windows fork starts from `arkits/road-rash` branch
`port/plan33b-windows`, commit `6b3b6be9ca5698b8b17f8f1ab53f2744a5202238`.
The audio reference is the reconstructed
[3DO source](https://github.com/trapexit/3do-decomp-road-rash/tree/cc33365427a7cc193c3b9111a24948e9993bb7ea).
This is an adaptation to the PC simulation, not a bit-exact 3DO emulator.

## Engine processing

`audio=3do` selects the original signed 16-bit samples, source sample rates,
and sustain loops. All bikes use sample 1; their gear profiles and velocities
determine revs. Sample 2 is traffic, 18 is boost, and 19 is the alternate
geometry-mode engine. The PC route's type-3 predicate selects the latter.
No DC removal, loop crossfades, EQ, or resynthesis is applied to the files.

The original 3DO gear-profile words match those in the corresponding PC
BIKESPEC records. The Windows executable maps SPEC 1–5 to EngRat, 6–10 to
EngSport, and 11–15 to EngSuper. Native mode no longer needs to bake three
different bike categories into WAVs: it applies the profiles at runtime.

The adaptation implements:

- Raw 16-bit frequency controls, with source-sample-rate scaling. At raw
  `32768`, one source frame advances per corresponding source-rate interval.
- A 44,100 Hz stereo mixer with sustain-loop endpoints and linear interpolation.
- Full 15-bit amplitude and the 3DO linear stereo-pan law, retaining phase
  when pitch, amplitude, or pan changes.
- Five independently modulated engine voices during the 180-frame startup,
  including player accelerator control; three engine slots during racing.
- The player and two nearest bike/traffic emitters, squared distance gain,
  camera-motion Doppler, and the Opera integer atan table for near-field pan.
- Gear-dependent rev targets, limited slew, airborne revving, and crash rundown.
  Native audio owns its pitch/speed caches and random stream: it does not write
  its modulation into networked simulation state or consume simulation RNG.
- The sampled 3DO combat/crash/footstep/skid effects and corresponding cinematic
  audio when an extracted track exists. Missing cinematic audio keeps PC audio.

Reference routines include `update_engine_audio_modulation`,
`update_rider_engine_pitch`, `update_racer_engine_audio_spatial_state`,
`update_rider_engine_audio_voice`, `update_traffic_engine_audio_voice`,
`start_audio_manager_voice_record`, and `process_race_audio_event_queue`.

Known fidelity boundaries: this does not execute the 3DO DSP instruction set.
The procedural ThingSliding, OffRoadSkid, CarHorn_Variable, RR_Siren and
RR_SirenBlast patches retain the existing PC equivalents. PC speech/menu
behavior remains in place. Spatial positions, render heading and event timing
come from the PC simulation; branch separation and 3DO voice-manager stealing
are not reproduced instruction for instruction. No recorded 3DO hardware oracle
proves a bit-exact mix. These limits are distinct from the native engine path,
which uses the extracted 3DO samples and frequency/gain laws directly.

## Extraction

Run with Python 3 and FFmpeg:

```powershell
python tools/extract_3do_audio.py "Road Rash (Europe).bin" "E:/RoadRash3DOAudio" --ffmpeg "path/to/ffmpeg.exe"
```

Both raw MODE1/2352 BIN and 2048-byte Opera filesystem images are supported.
The extractor preserves native containers, emits the 19 named race WAVs,
carves embedded AIFF/AIFC samples, and decodes nonlicensed movie audio.
The manifest records source/image hashes, loop endpoints, source rates,
and conversion results. WAV `smpl` endpoints are inclusive; the 3DO end
marker is exclusive. Sampled race PCM is verified byte for byte after
endianness conversion, with its frame count unchanged.

The supplied European image yielded 19 race samples, 162 additional sampled
sounds/instruments, and 39 movie-audio WAVs. Fifteen bike-shop streams contain
video only: their engine previews are generated live. Fourteen licensed
background tracks, the licensed music videos, and the intro container are
excluded (17 containers total). No soundtrack material is downloaded by the tool.

The five EAMS files are event sequences, not recorded audio. They are converted
to standard MIDI using the OMS 240 Hz clock and explicit duration note-offs.
Optional `--fluidsynth path/to/fluidsynth.exe --soundfont path/to/bank` also
produces WAV previews. Those previews use the supplied GM bank; they do not
claim to reproduce the original SCON DSP instruments. Original SCON banks and
their embedded instrument samples remain available in the extraction output.

No extracted audio, soundfont, disc image, original executable, or other
commercial asset belongs in this repository or in release downloads.

## Configuration

The installer writes `rr-port.ini` beside `rr-game.exe`:

```ini
audio=3do
3do_audio=3do_audio
renderer=dx9
```

Use `audio=pc` for the original PC sample-selection and DirectSound-style
gain/pitch path. A missing or invalid 19-file 3DO bank automatically selects
PC audio. The installer exposes both modes; it defaults to PC when no complete
3DO bank is supplied. `RR_AUDIO_MODE`, `RR_3DO_AUDIO_DIR` and `RR_RENDERER`
environment variables override the file for development.

## MIDI and presentation

Race MIDI uses a built-in FluidSynth synthesizer, rendering into the game's
own mixer. It does not send MIDI to an external Windows MIDI device. The
General MIDI GRUNGE arrangements are used rather than the partial AWE/SBK
arrangements. Soundfonts (`.sf2`, `.sf3`, `.dls`) in the asset installation,
its `soundfonts` folder, the executable directory, or its `soundfonts` folder
are tried before Windows `System32/drivers/gm.dls`. No soundfont is distributed.

The Direct3D 9 presentation wrapper draws through the existing SDL main HWND.
It preserves the simulation's floating-point settings and handles device loss,
resize, vsync, menus, race frames and movie textures. `renderer=sdl` keeps
the ordinary SDL presentation backend available for troubleshooting.

Movies already use the main window. Native file dialogs have been removed
from game asset import. Save/load uses the game's own menus, enumerates both
`.RRS` and `.rrs` files in `saves` beside the executable, and paginates the
entire list. The installer imports existing saves without overwriting them.

Windows distributable builds omit the upstream disc-derived application icon.
The packager audits the executable resources and rejects that embedded icon,
in addition to its strict file allowlist. `RR_ORIGINAL_APP_ICON` defaults off.
