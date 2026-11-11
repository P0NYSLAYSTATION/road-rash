# Native original-game captures

Only `E:/RR/ROADRASH.EXE` in the exact `E:/RR` installation is supported by the
native runner. Private-copy preparation/component launch paths were removed.
The reference-derived working directories were deleted at the owner's request.
The executable is never edited. SHA256 is
`b845fc4053f2387c863d3fd71d4b1457cefc2eab900d9562511d8d403b6f7563`.

The actual working AWEMAN32.DLL, SHA256
`ca7f2f65bf6d1550857556a27d65d4f44d7a6ca14c2be89b7c0f9affb6a47022`,
is backed up before adding a PE entry section that loads RRCAPTURE.DLL.
Original DLL section bytes and exports remain intact. The embedded Windows
helper has no AWEMAN_REAL.DLL dependency. Journals and backups live in
`artifacts/oracle-live-backups/`. Every exit path restores the original DLL,
attributes and timestamp. Accepted output is published only after restoration
and checksum verification succeed.

Explorer launching the exact executable was verified through Computer Use:
startup credits and game menus rendered. A command-runner launch of the same
executable instead failed. The original startup path at 0x00448b90 calls
RegOpenKeyExA(HKLM, game key, KEY_ALL_ACCESS); the measured command-runner result
was 5 (access denied), which branches to the installation-error dialog at
0x00448f3b. The runner copies the installed settings into a temporary HKCU key
and redirects only that executable's game-key imports. The machine installation
settings remain untouched. The command-runner startup then opens successfully
and runs complete captures. This establishes the failing operation; it does not
claim to explain every Windows policy difference between the two launchers.

```powershell
oracle/build-windows.ps1 -Kind capture
$env:RR_ORACLE_SIM = '1'
$env:RR_ORACLE_HUMAN = '1'
$env:RR_ORACLE_DRIVE = '1'
$env:RR_ORACLE_COURSE = '4' # original course order: 4 is Napa, 1 is City
$env:RR_ORACLE_SEED = '1'
$env:RR_ORACLE_FRAMES = '600'
python tools/oracle_capture.py capture windows-napa-human-600 --install E:/RR --live-hooks oracle/build-windows-capture/AWEMAN32.DLL --timeout 45
python tools/verify_sim.py --mode acceptance --binary build/debug/oracle-sim-test.exe --assets E:/RR --fixture analysis/oracle/windows-napa-human-600.tsv
```

The receipted `analysis/oracle/napa-human-2400.tsv` capture now passes strict
acceptance: 2400 ticks identical, including RNG and all captured fields. The
original helper records actual human-control branch invocations at 0x434660;
legacy captures without that proof remain ineligible for human acceptance.

Both original and native initialize the MSVC RNG from the capture seed. Field
replay executes tick 1 before auditing its POST-step RNG, then verifies the
green PRE-step state and free-runs. City seed 1 has first-frame state
742760408 on both sides; three countdown pedestrian draws account for the
difference from factory completion. Acceptance never reanchors each tick.

Expanded City pedestrian and signal comparisons currently stop at tick 514,
where original graphics resource eviction remains unported. The old file
named windows-napa-startup was actually City; its spawn divergence is fixed
by circular AI lookahead. See `docs/agent-handoffs/windows-parity-first-20261111.md`
and `docs/VERIFICATION.md` for current evidence and limitations.

The replay executable now catches exceptions and writes them to stderr instead
of leaving an unattended Windows C++ runtime dialog open.
