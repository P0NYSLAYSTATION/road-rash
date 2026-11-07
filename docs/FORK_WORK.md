# Windows fork work

Base: arkits/road-rash, port/plan33b-windows, 6b3b6be.

Requested sequence:
1. Extract non-licensed 3DO audio locally.
2. Convert sampled audio to WAV with loop metadata.
3. Create the owner fork (complete).
4. Clone on E: and configure VS Code/MSVC/Ninja (complete).
5. Add selectable 3DO audio processing, preserving the PC backend.
6. Built-in FluidSynth MIDI with installation-local soundfont discovery and Windows gm.dls fallback.
7. Direct3D 9 presentation through the existing main window.
8. Keep movie playback and save/load in the main window; remove native import dialogs.
9. Build and validate on Windows.
10. Create and publish an asset-free installer with local PC/3DO asset import.

No commercial audio, soundfonts, disc images, or original executables may be committed or packaged.
The local developer presets use MSVC and Ninja like D:/NinjaDM/source, with x64 for this native port.
The full upstream networking configuration remains available; the local offline development preset omits optional network transport dependencies.
