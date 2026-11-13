# Windows branch integration and release correction — 2026-11-13

Windows merge `85ca5ec` has parents `2f14b58` (all 41 unfinished Windows files
preserved in a local checkpoint) and `967e1c2` (parity history). It was pushed to
`port/plan33b-windows`; the unwanted simulation branch was deleted locally and
remotely. Ten conflicts were resolved by integrating both implementations.

Snapshot authority retains independent player collision regions. The parity
physical-half, pool-order and collision changes remain. Snapshot schema 2 now
transfers the additional ring, actor lifecycle and retained scheduler state.
The real-course snapshot check compares the restored state and 120 future ticks
without RNG reanchoring. Snapshot, snapshot-server and snapshot-native pass.

Merged all-target Release build succeeded. Six strict original capture comparisons
pass: five finished level-1 races (143,764 ticks), plus 94,929 level-5 Hiway ticks
ending in arrest, not a finish. Evidence: build/windows-merge-race-parity.json.
The first native suite passed 146/156; ten police cases failed solely because the
new checkout lacked their local fixtures. Existing captures were then supplied
at the expected local paths; the rerun is recorded separately. No missing test
was disabled. Owner-excluded menu pixels and widescreen remain outside this gate.

Installer packaging now requires a clean port/plan33b-windows checkout. Older
1.1.2–1.1.20 artifacts were published from the wrong development branch and lack
the unfinished Windows changes integrated here. Their code is now in Windows
ancestry; that does not retroactively change their installer bytes. Version
1.1.21 will supersede them with the actual merged Windows implementation.
No original executable capture was launched for this integration.
