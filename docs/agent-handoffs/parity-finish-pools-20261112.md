# Complete Hiway parity and continuation

Baseline74e1c322056d9a002b359087b19d5cbc9416b7db, branch codex/simulation-parity.
That checkpoint is public release1.1.13; public installer checksum
75d0192b1f97c2becc3a45020a3f5d955c2c75f665c3167df6cc91eb4519a518 verified.
Isolated1.1.12->1.1.13 update preserved600 files and SDL menu smoke passed.

## Measured acceptance

Unchanged completed original Hiway14084 ticks pass strict whole-race acceptance,
including RNG, one seed anchor and no state hydration/reanchor. Trace SHA256
0fe2e9d7dd984990c528ee4b05cf80015c37b2e7a84b2c442e0b524e27d5975d.
Command: python tools/verify_sim.py --mode acceptance --whole-race --binary
build/release/oracle-sim-test.exe --assets E:/RR --fixture
analysis/oracle/hiway-finish-streamed-rng-l1-s1.tsv --report
build/hiway-finish-place-animation.json. Report acceptance_met=true, exit0,
14084 ticks identical, original finish outcome3. Scope: this human level1,
seed1, control sequence. Other tracks, levels and alternate routes remain open.

## Changes and evidence

- Park keeps physical graph node/local. Original11159 rider9 local26614 versus
  old26358; original hiway-rider-wake-11175.tsv SHA
  160d69c6b723d0229a6042e8e9eda3122db8b25909472848febd1837849fd380,
  11175 validated controls, DLL restored. Exact prefix advanced11158->12063.
- AI straight target reads next physical slice lane word.00409225/0040923c
  load+2c. Resolves12064 target95488/native31488 and steering sign; prefix13471.
- Pedestrian pool retains raw motion and crossing lane. Generic init_object
  resets only original explicit fields; elapsed time and projection state still
  reset. Initial broad retention omitted+18 reset and regressed5554, fixed.
  Retention alone still missed13472 because native first-free pool order differed.
  Original tail insertion/removal is LIFO; track recycled ranks and reuse last
  returned slot. Recycled image and free order now participate in state_hash,
  including inactive slots. Snapshot copy already preserves system state.
  Prefix advanced13731. Synthetic test checks retained velocities/lane, reset
  speed/heading/elapsed time and choosing the last returned slot.
- Finished actors keep scheduled AI/motion and collision eligibility based on
  original flags. Removes unmeasured logical finished short-circuits. Resolves
  13732 AI/throttle and13733/13835 contact separation, prefix13922.
- Live half mover00406890 latches finish at its scheduled position, without
  clamping preceding bike motion. Resolves13923 overshoot60, prefix13983.
- Animation reads finish place004c7ff0; only top3 select victory. Default zero
  incorrectly selected animation5 for the player's later placing. Resolves13984
  and matches through the finish outcome at14084.

Fresh original hiway-ped-motion-finish.tsv SHA
52c902fbb7af3729d0d30ecffff92bce1abce5c16237d61d6fda655d825a4a5c contains
14084 validated controls, original process exit0, working DLL restored. It was
captured without UNTIL_FINISH, so no finish-outcome marker is present; do not
call it independent whole-race acceptance. Fresh-prefix strict comparison passes all14084 ticks in
build/hiway-complete-fresh-prefix.json. An attempted --timeout240
verification command was rejected by argparse (unsupported option), no test ran;
rerun uses supported arguments. Original fixed fixture establishes whole-race.

## Validation and next work

MSVC full Release build succeeds. SDL native gate104 passed,35 explicit skips,
zero failures; two unavailable Direct3D tests excluded. Logs
build/hiway-complete-full-build.log and hiway-complete-native-tests.log.
Strict Napa2400/City600 pass: build/hiway-complete-regressions.json.
First finished-contact synthetic fixture incorrectly ended the entire race;
corrected to retain an unfinished player. Its separation can be longitudinal,
so regression checks geometry separation rather than assuming lateral axis.
Full gate passes corrected tests, including race, pedestrian and race_state.

Moving-course audit confirms Napa1259 rider4 position43171/native43140 and
City1251 pedestrian state3/native0 remain open (build/hiway-complete-moving-course-audit.json).

Next: commit/push this
checkpoint, clean rebuild, installer1.1.14, isolated update from1.1.13 and publish.
Then generate/validate completed recordings for other courses and resolve their
first divergences. Multiplayer remains held. Only E:/RR/ROADRASH.EXE with its
working AWEMAN32.DLL is permitted for original captures. No original currently
active; last capture restored. E:/road-rash multiplayer WIP untouched.
