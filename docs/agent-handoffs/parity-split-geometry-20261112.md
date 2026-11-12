# Finish traversal and physical split geometry

Baseline:2d696bf, branch codex/simulation-parity. Original runtime remains
E:/RR/ROADRASH.EXE with reversible hooks in the working AWEMAN32.DLL.
The separate E:/road-rash multiplayer work is untouched.

## Measured changes

The immutable completed original Hiway capture remains
`analysis/oracle/hiway-finish-streamed-rng-l1-s1.tsv`, SHA256
`0fe2e9d7dd984990c528ee4b05cf80015c37b2e7a84b2c442e0b524e27d5975d`,
14,084 ticks, finish outcome3. No reanchor or golden modification.

-00407410 uses preferred graph branch byte18 before a finish line is known.
 00454253 discovers the absolute finish when its section is first built.
 Native route-A distance was5120 too large at6007; that difference is resolved.
-00449ce0 requires self/candidate slice+36 low-bit overlap. Adding that
 missing AI scan gate resolves the next difference at6056.
-00452e10 steps its iterators even for a zero residual on section entry.
 CESR activates the first guardrail event there. Preserve untouched sample0
 separately for initializer/component fixtures; live forward routes use the
 stepped start. DLSR/NALR zero-length initial intervals also take their endpoint.
-00454890 narrows both inner shoulders using the measured traversal-window
 countdown and separation. The simulation now stores absolute shoulder edges
 in physical128-slot ring storage, copied with snapshots, and uses them for
 live walls, surface classification and ground height inside the active ring.
 At6254 original and native now both clamp lateral72704, bounce steer rate281
 and clear yaw60. The original entry was64979/rate-720, type4/event5,
 inner shoulder250; raw native shoulder26 was wrong.

The6552 lateral-target gap (brain+194, not forward target+198) is also resolved.
Original linked-slice capture proves discarded half1 ends with a self-link at
the committed window's last slice; curve lookahead must repeat that slice.
Native lookahead incorrectly read the next course sample. Physical ring slots
now preserve this stop independently for each half. The completed recording
matches ticks1..6556 including RNG. First remaining gap6557: captured rider3
steer original2310/native3465 (native rider7). Evidence:
`build/hiway-physical-inner-shoulders.json`,
`build/hiway-6552-target-lateral.json`, and `build/hiway-closed-split-link.json`.

## Capture evidence and verification

`hiway-matched-split-wall-6500` replays exact hexadecimal control words and the
measured initial extra-AI speed table. Complete6300-tick wall-call probe
`hiway-wall-call-6300.tsv` SHA256
`3cdb3fc689455ca40bcb5b94b783c4be0dfaf158d4c0b474d502200d279a2bbd`
proves entry/exit of0043a610, including6254; its receipt confirms DLL restoration.
These are diagnostic prefixes, not new completed-race acceptance.

`hiway-ai-target-6600.tsv`, SHA256
`3da2bb577d2ca91c34dec5bfd9c7e209f0e2dd590dad9ec18ef3a4cb8bc6e4c9`,
records the original6552 target164096 and the linked sequence ending in repeated
slice4911164. Its older probe label's first lanes value is actually raw+28
(a pointer), not a lane width; ignore that diagnostic column. Future probe
output reads left width+30. All6600 recorded controls validate and the working
DLL was restored to ca7f2f65bf6d1550857556a27d65d4f44d7a6ca14c2be89b7c0f9affb6a47022.

The old separate .controls writer opened a file but wrote no entries. Every
control was retained in the TSV, so accepted replay data is intact. The writer
now saves/flushes one hexadecimal word per tick; capture validation compares
the entire companion file against TSV controls before publication.6500 and6300
recorded words match their respective traces. Tooling regression:25 tests,
two platform skips, zero failures (`build/recorded-controls-tooling-tests.log`).

The established Napa2400 and City600 fixtures remain exact
(`build/split-geometry-regressions.json`). Synthetic split-shoulder and copied
ring checks pass (`build/split-shoulder-regression.log`). Full gate results and
release validation are recorded in VERIFICATION.md as completed.

## Remaining work

Trace6557's steering gap after the physical split self-link correction.
Independent moving Napa1259 and City1251 gaps remain. Medly's earlier wreck
does not establish finish coverage. Complete all-track start-to-finish parity
is still open. Render-side split profile integration, arbitrary reverse entry,
all retained physical-slice fields, asynchronous family writes and projected-x
pedestrian loading remain scoped gaps; do not infer them from this prefix.
