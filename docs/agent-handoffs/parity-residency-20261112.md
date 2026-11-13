# Parity continuation — resource residency and circular slices

Baseline: a4bf3e6, codex/simulation-parity. Preserve the pre-existing local
scheduler, countdown/contact, replay-memory and capture-tooling changes; these
belong to the same ongoing parity task. E:/road-rash multiplayer WIP is untouched.
Last published installer is still 3do-1.1.2. Only E:/RR is the original runtime.

## Measured progress

The exact completed original capture is
`analysis/oracle/hiway-finish-streamed-rng-l1-s1.tsv`, SHA256
`0fe2e9d7dd984990c528ee4b05cf80015c37b2e7a84b2c442e0b524e27d5975d`:
14,084 ticks, outcome3, 52,137 continuous RNG caller entries, no overflow,
working DLL restored. Logging flushes each tick and only reuses RAM afterward.

- Initial native replay failed at430, loaded-resource flag, not RNG.
- 00441445 checks the cached RRAN header, not whether the family selector now
  resolves to a different family. 004155e5 overwrites an existing family-slot
  allocation without clearing the unused tail. Hiway family180 is175276 bytes;
  replacement182 is112744. Selector195's RRAN starts146528 and survives, while
  selector194's RRAN at112172 is overwritten. Evidence:
  `build/family-tail-investigation.json`; independently decompressed payloads
  pass their stored CRCs. CourseScene now overlays writes at the cached header
  offset using its family event timeline; the pedestrian factory uses that
  validity query. Focused real-asset regression passes. First gap moved430->643.
- At643 an old pedestrian receives its last scheduled move before deferred
  unlink. Its retained +140 pointer addresses a reused physical road slice.
  004394ee reads that slice's +b4 curvature. Logical sample50 now aliases178
  in the128-slot ring (back sample52). Native motion had read historical50.
  Ring-backed accumulation now follows the physical slot. The same strict,
  single-anchor replay passes ticks1..5658, then fails a missing scheduler
  node at5659. Evidence: `build/hiway-streamed-rng-retained-slice.json`.
- Napa expanded human2400 and City signal600 both still pass exactly after
  these two fixes (`build/pedestrian-residency-corpus.json`). This is prefix
  coverage, not all-course/start-to-finish acceptance.

## Further measured fixes

5659's missing node is the in-road BORR pool, not an intersection. Original
0045461f..00454649 inserts a template00476cb8/type1 node for each BORR lateral
strictly inside the slice's road edges. Pool00476ce8 has20 slots; template
flags0 become4 on insertion, interval20, all callbacks NULL. Thus it is in
AI scans but not pair-contact gates. Native previously had geometry collisions
without these scheduler nodes. Added pool allocation, list membership, AI-scan
entries, twenty-tick null-callback cadence, deferred removal, snapshot/hash
fields, and capture comparison. A component regression checks strict bounds,
capacity, mid-slice positions, cadence, deferred unlink, and no RNG consumption.
The component regression passes; the exact replay moves5659->5818.

5818's opposite-half pedestrian had original spawn lateral64000/native0.
RingActor now preserves its physical walker offset and selects the record's
roadway identity, rather than inheriting the player's current half. The next
gap was a retained owner-link event: the car probe had hard-coded0. It now
reads TrackRing's actual circular-slot event. 0045312f ->0045363f/00453645
sets5/6 on interior dual samples in either direction; the native forward path
had omitted this assignment. New split and reused-owner-link regressions cover
these fields.

5968's lazy reload exposed a section without any slot3 load. The original
retains its incoming family; the scene reset it to0. Route-aware look resolution
now inherits untouched slots and header bytes from preceding sections. A real
Hiway regression checks the transition from section0 to5. Strict replay now
matches through6006; first gap6007 is rider2's finish distance227708/232828.
Report: `build/hiway-streamed-rng-inherited-families.json`.

00407410 reads the global finish only after it is nonzero and the actor has a
live slice. Otherwise it walks the graph from the current/parked node, using
split byte+18 and merge+16. The native factory currently sets a route-A finish
immediately. Hiway root remaining1810 samples; entering branch nodes284/92 has
910/890 samples remaining respectively. The20-sample difference explains the
5120 Q8 divergence at6007. Next: port graph remaining distance and finish-line
discovery (00454253..00454274), then recheck this same original recording.

## Remaining limits and next checks

Family validity follows the route's forward family timeline and retains
cross-section buffer tails. Backward traversal, asynchronous loader completion
and two simultaneous branch-family writes still
need evidence before claiming complete resource-memory equivalence. Projected-x
draw gating remains unported. Do not discard resource flags from comparison.

The first retained motion correction is measured on an old pedestrian's final
move; road-half identity on reused split slots and other retained slice reads
still need targeted comparison. Never change the golden to hide these gaps.

Known independent gaps: Napa moving capture1259 spurious car contact; City
moving capture1251 pedestrian state3 versus0. Original uses one mixed contact
walk; separate native car/pedestrian passes remain suspect. Medly's original
scripted run wrecked (outcome5); it is not finish coverage.
