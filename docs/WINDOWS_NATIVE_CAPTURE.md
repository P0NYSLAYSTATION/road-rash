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

Completed native captures include 600 human-control Napa ticks and a fresh
120-tick restoration test. The latter's receipt explicitly records phase
`restored`, complete contiguous ticks and the compatible executable hash.
The 600-tick replay matches all compared native fields and RNG, but strict
acceptance correctly remains red: 23 extra original diagnostic fields are not
yet compared. Do not describe this as full parity. A preliminary 120-tick file
named windows-napa-startup actually used course 1 (City) and diverges at spawn;
its misleading filename is historical, not Napa evidence. Exact logs:
`build/windows-napa-coverage.log`, `build/windows-napa-human-600-parity.json`,
`build/oracle-napa-restoration.log`, `build/original-startup-registry.tsv`.

The replay executable now catches exceptions and writes them to stderr instead
of leaving an unattended Windows C++ runtime dialog open.
