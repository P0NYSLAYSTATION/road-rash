# Post-release gameplay parity work — 2026-11-13

Baseline: 5397a34; release 1.1.19 is published and verified. Current edits are owned by this session in CMakeLists.txt, race implementation/header/schedule/screen/state/pedestrians and race/field replay tests.

New result-phase checks exposed early result transitions. Added per-rider registered result codes with original offline top-three qualification, finish braking/crashed handoff and wreck recovery registration. These latest changes build but still require complete regression validation. Earlier five-finish passing reports predate this latest implementation.

Required immutable Medly seed-one recording analysis/oracle/medly-finish-keyboard-l1-s1.tsv contains 13,271 ticks and ends in wreck outcome 5, not finish. Added mandatory Windows oracle-sim-wreck registration (not yet reconfigured). First failure at tick 1714 was graphics eviction incorrectly resetting pedestrian animation. Original eviction retains animation scalar state; current fix only releases selected graphics. Replay now reaches tick 3659, where rider 3 collides with a car and native transfers impulse instead of crashing. See build/medly-wreck-registered-result-acceptance.json and build/medly-wreck-tick3659-diagnostic.log. Car raw speed versus cached scalar is an unproven hypothesis; inspect handler assembly and live values before changing.

Additional missing fatal-ground registration branch at original 43a45a/43a4d3 is under investigation; exact thresholds not recovered yet. Logs build/original-lethal-ground-disassembly.txt and original-result-registration-disassembly.txt. Do not claim it implemented.

No original capture is active. Only permitted original runtime remains E:/RR/ROADRASH.EXE with cwd E:/RR and user's working AWEMAN. No reference install. No menu pixel or widescreen work; multiplayer held until gameplay parity. No new release is ready. Next: resolve Medly tick3659 and all subsequent strict divergences, then validate five full finishes, ten police cases, long L5 and focused/full native tests against latest code before packaging next release.

## Car-contact correction and current validation

Recovered 00404ed9 subtracts the other object's +108 world velocity, not +ec engine speed. contact_car now uses car.motion+108. The same handler's 00404b2c clears half+1e9 for uncrashed non-scenery impacts; car path now does so too. Synthetic longitudinal contacts with identical engine speed and opposite world velocity distinguish crash from speed transfer; car_contact-test passes (build/car-world-velocity-test.log). First synthetic fixture was a side impact and correctly did not exercise class2; changed its previous longitudinal placement to create the intended longitudinal impact.

Strict immutable Medly replay now passes the car crash but fails tick5622: ped serial26 state original0/native3 after native scenery contact. New read-only native diagnostic added to oracle/aweman_proxy.c under existing AI_TARGET_TICK probe, records dimensions/callback/flags/scenery counts. First short capture medly-ped-contact-shape-20261113 completed5650 ticks but used an incorrect probe environment name, so lacks extra shape logging. Second medly-ped-contact-detail-20261113 uses correct RR_ORACLE_AI_TARGET_TICK=5622; capture session78975 may still run, same original E:/RR with reversible instrumentation, deterministic drive2 seed1. Headless comparison of first short recapture session29808 also pending. Focused13 tests session53099 and five whole-race replays77386 validate current raw result registration +car fix; inspect outcomes before claiming pass. No installer/release ready.

## Focused gate complete

build/result-registration-focused-tests.log: 13/13 pass, zero failures, 135.88s; race-screen, race, car_contact and all ten required police cases. This gate validates raw registered results, car world velocity and car flash clearing. Additional equivalent flash clearing for solid scenery/pedestrian bike contacts (same original00404b2c) and BORR overlap trace logging are source edits AFTER this build and remain unvalidated. Five-whole job77386 still executing previous build; do not relink until it ends.

Second original diagnostic completed5650 ticks with shape fields; dimensions width4096,length16,height7680, callback440760, flags7 for serial26. It confirms original does retain a collision callback. Third original diagnostic medly-ped-static-box-20261113/session43482 adds exact rectangle coordinates under same probe, still may run. First recapture's strict replay fails earlier5307 half-mark byte (192 original vs960 native), consistent with missing flash clear in solid static handler; now added but not yet built. Do not claim recaptures reproduce the identical old RNG stream: direct comparison shows RNG differs by tick492. Each trace must be independently initialized and replayed; all captures use seed1, but original actor initial cache/state varies. Never overwrite immutable old fixture.

## Further measured work

Five-whole job77386 COMPLETE0: build/registered-result-five-acceptance.json,5pass143764ticks includes actual registered result timing, car velocity and car flash clearing (before latest solid-static/pedestrian flash edits and fall branch). Focused13pass already recorded.

Added recovered 0043a40b..0043a50b fall branch to vertical, with optional half-owner linkage. First departure from contact and raw above>=dt*17920 triggers bike reason6 with vertical impulses rider{0,0,-80},bike{0,0,80}; type1 ground additionally knock_off huge damage/register5; detachedhalf triggers onlytype1. Independent threshold17919/17920 xterrain0/1 components pass after explicitly setting synthetic actor type4 and dt1. Initial failed fixture had kind0, so did not exercise original bike-specific branch; implementation threshold unchanged. build/fatal-ground-boundary-test.log0fail. Latest changes need broader replay validation.

Original captures shape/detail/staticbox/target all complete and working installation restored after each. Latest target sample539 source538 has base-532, rectangle{-3584,-19456,3072,512}, matching native original-wreck overlap. The source sample538's garbage cached box is NOT sample539 and does NOT establish the cause of old tick5622 discrepancy. No speculative family-memory fix applied. Old capture vs new runs differ in initial AI state/RNG timeline (first stream difference492) despite same declaredseed1; do not treat recapture as reproducing identical old state. New strictprefixes after static flash fix are running (exec584...). The old wreck remains required/failing at5622 until actually resolved. No release20 claimed.

## Reproducible old-state diagnostic now active

Recovered existing documented mechanism (field-parity-20261018.md): six extra-AI three-level speed jitters run before race seeding. First-frame raw_1e0 maps row=(address-463c78)/40; captured brain_1bc/1c0/1c4 provide18 startup values. build/medly-wreck-matched-startup-speeds.json contains81,103,119,85,101,119,85,105,122,83,111,125,93,109,123,82,101,124. Extracted all13271 old controls into build/medly-wreck-original-inputs.controls (old companion empty). This supersedes unexplained initial-state notes; neither live RNG nor state is reanchored.

Capture87000 active medly-wreck-matched-contact-20261113: original E:/RR, helperoracle/build-windows-scenery-target,5650ticks,drive0,inputabsolute oldcontrols,EXTRA_SPEEDS valuesabove,AI_TARGET5622,SLICE539. It should reproduce old tick5622 exactly; verify before inferring. Previous unpinned diagnostic prefixes both PASS (build/current-medly-probe-prefixes.json2pass; frames5650+5623=11273), with latest fall/staticflash implementation. Original target539 rectangle matches native coordinates, so no speculative mutable-family collider changes were made. All captures remain retained, none replace old goldens.


## Matched capture complete; mutable scenery fix in progress

Matched capture87000 COMPLETE0, traceSHA3345e664f846bd673826312d06d857c918e47b51b987d52348bbf7ec82c95da0. build/medly-matched-stream-check.json verifies all5650 RNG states, controls, player positions and speeds exactly match immutable original recording. This is the meaningful diagnostic, unlike unpinned jitter reruns. Original working AWEMAN restored SHAca7f2f...47022.

Exact tick5622 serial26 sample539 BORR-532 boxpointer51695208 contains{-1583238479,-1314807391,-30736127,-1551596404}, observed both by read-only slice lookup and actual4373f0 sweep hook. Original sweep leaves state0. Native retains immutable oldrectangle{-3584,-19456,3072,512}, imposing state3. This confirms reused family-buffer bytes alter the cached collision rectangle.

Current edits add AnimationFrame collision_source_offset (absolute byte position inside family payload), CourseScene::static_collision_boxes resolves each byte from newest covering payload in ring family history with shorter-replacement tails preserved; Race optional static box provider follows BORR selector slot and billboard selectors. Existing box count/pointer offset remain as cached; no arbitrary collision suppression. Signed lateral additions now explicitly wrap for overwritten bytes. First build/replay of this implementation was actually rejected by auto-review usage-limit failure; after user's explicit continue, retried tool successfully (session65671). build/mutable-scenery-box-build.log successful; strict full13271 wreck replay still pending build/mutable-scenery-medly-wreck.json. No claim fix validated yet. Synthetic embedded-animation test now verifies box source offset includes64-byte resource embedding; not yet built.

## Mutable boxes validated to tick10240; rank-mask investigation

First strict replay after byte-overlay fix (build/mutable-scenery-medly-wreck.json) advances immutable wreck trace from5622 to10241; all10240 preceding ticks exact. Thus original bogus rectangle no longer creates false pedestrian state3. Next failure only rider5 place original9/native10 at10241. Native comparison log build/medly-wreck-place-comparisons.log: rider10 behind at286861 but treated separate roadway, so its shorter finish distance174451 versus177207 counts ahead. Original407550 reads physical sub-slice+36 masks, not static route annotation. Ring common-half rebuild can change physical half0 mask to3 while logical route remains dualhalf0.

Capture57439 active: medly-wreck-matched-place-20261113,10245ticks, same old recordedINPUT and startupEXTRA_SPEEDS,seed1, originalE:/RR; helperoracle/build-windows-place-mask adds read-only4075f0 call-site probe to print masks/positions/distances before actual refresh. No originalprocesselsewhere.

Current unvalidated mask change: TrackRing retains per-physical-half roadway_masks alongside motion_valid, writesdual1/2 orcommon3 onlyforhalvesactuallybuilt (unusedhalf1retainsoldmask). Race::refresh_place nowreads thatphysicalmask withlogicalfallbackforsyntheticnorings. Build/replay31778 active, logsbuild/physical-place-mask-build.log andphysical-place-mask-medly-wreck.json. Await originalprobe confirmation and strict results before claiming fixed.

Additional diagnostic consistency: static_slice_state now uses the same resolved mutable boxes as runtime collision. Generic resolved_static_boxes centralizes optional provider fallback. Embedded-animation source-offset test not yet built; no completegate/package20 yet.

## Original rank proof and next shared-mask consumer

Original matched-place capture57439 COMPLETE0,10245ticks SHA04032e9791383fd0ecf1c70e20f6d8f0bd4d1ec4342c06087b2595863544059b. At10241 actualself(pos288384,fd177207) physicalmask7 (lowbits3); other rider10(pos286861,fd174451) mask2. Original compares position, so rider10 is behind. Native logical masks1/2 incorrectly compared finishdistance and addedoneplace. Physical-mask replay31778 now passes10241, next failure10245 AIbrain168 originalrider5/native-1 forrider10, same erroneous scan-next mask source.

Current change stores retained physical slice_position and roadway_mask in mixed ScanObject; scan_next and4368f0 contact gate use physicalmask intersection, replacing static route annotation/route-id approximation. Crash half uses its actual retained CrashActor::sample. This changes no RNG directly. Replay15372 active with latestcode; build/physical-neighbor-mask-build.log succeeded, reportbuild/physical-neighbor-mask-medly-wreck.json pending. Wrapped far-scenery box width multiplications (32-bit original imul) now handle overwritten bytes without signed overflow. Snapshot diagnostic accessor reads identical mutableboxes as collisionruntime.

Focused animation embedding and physical-mask collapse/snapshot tests58698 COMPLETE0:2/2pass, build/family-offset-physical-mask-tests.log. These precede last ScanObject mask-consumer change; new full native/five-race gates stillrequired. CMake mandatoryoracle-sim-wreck command now explicitly passes RR_TEST_ASSETS_DIR, avoiding an absent default disc path. No originalprocessactive after57439 restoration; no newrelease/commit yet.

## Tracker diagnosis supersedes the initial expiry guard

Physical neighbor/collision mask replay15372 passes11774 ticks; first11775 has7native/6original slots: rider4 falsely wakes. Original matched tracker probe64051 COMPLETE0,11776ticks SHAafac438e9b0afe1d595694ded7cd21ff51c612415226749544fbfc3665f11dc3. Contrary to the initial simple expiry interpretation, original node124 graph references BOTH point at SAME retained self-linked sub-slice, local77568 (303*256). Rider4virtual61802 is below that lower bound, so doesnotwake. Actualother commonnode188 refs are61952..81664. Original450e3f clearsboth ontracker0selflink, but other hand-over paths450e95..450ec4 and450f59..450f7c can reattach references; simply treating a closed node as expired is incomplete.

Current temporary guard inmove_bike_parked `if (w->closed_upper(node,t0)) continue` prevents first falsewake, advances strict replay43655 to11978, then misses a legitimate wake ofrider2 (original7/native6). Do not claim thisguard correct/complete; trace actual node tracker hand-over and replace it. Nativeold first11775 falselyderivedlo0becausephysicalt0node188 versusvirtualnode124; originalpreservedlower77568. At11978 originalnewlive rider2pos349824; needoriginalprobe at11978 tolearnpointerupdates/virtualnode. Native diagnosticsbeforeguard inbuild/medly-wreck-tick11775-diagnostic.log.

Original450dc0 tracker objects carrykind+0c,localfraction+10,actualphysicalslice+14; unlinkold graphnode+18/+1c, follow+08forward/+18backward, resetlocalonnodechange, cloneotherphysicalhalfwhennewsectiondepthincreases; specialhandover can set otherkindreference. 450d30 allocates tracker;450f90 walks eachkindlist preservingnextbeforestep. Setup455503..455558 creates both atbackprimaryslice, advanceskind1by91slices thenkind0by14. TrackRing currently re-derives both fromglobalback offsets; this isexplicitremainingapproximation toreplace, not bless asparity. See analysis/original-disassembly.txt450d30..450f90,455503... anddocsoldnotes. Existingclosed_ends captures discarded self-link endpoints butnotalltrackerrefs.

## Persistent tracker implementation replaces the temporary guard

Original11978 matched hand-over capture50520 COMPLETE0, SHAe43fd6c9526ff8a19d50bfdbceec3ad87eceaa5b2c14c1a213c421a9c441168e: discarded node124 retains lower77568 and upperadvances85248; rider2local77824 wakes correctly. At11775 bothrefs77568, so rider4local61802 remainsparked. This confirms retained boundary references, not a permanent closed-node exclusion.

New src/track_ring_trackers.cpp ports the four-object pool00450d00, two kind lists, source-slot references and graph-node bounds, split-window cloning00450dc0 and opposite-kind hand-over450ebb/450f77. TrackRing setupcreatesbothrootsatbackthenstepsupper91/lower14, advanceusesoriginal454120 order lower-before-frontbuild/upper-after forforward, inverseforbackward. Wake reads actual graph-node refs; temporaryexpiryguard andback+14/back+91 approximation removed. Physicalforwardselflinks use existingclosedbit; genericnextslot/half transition currently needs wider forward/backward validation, so do not claim complete tracker fidelity yet.

Strict replay24805 build/native-tracker-lists-medly-wreck.json advances to12461: all12460 prior ticks exact includingbothfalse/legitimatewakeevents. Remainingrider2brain194 target original60672/native61184 (2worldunits). No fullwreckoutcomevalidatedyet. New tracker unitgate build/native-tracker-list-test.log PASS: initial14/91 offsets, discardedlo24/hi81, trailing cleanupclearsboth, snapshotrefs preserved. Tracker state is now included in RaceInstance state_hash; thislastadditionnotbuiltyet.

Originalcapture87263 active medly-wreck-matched-curvature-20261113: exactE:/RR, matchedstartupandoldINPUT,12462frames, AI_TARGET12461, helperoracle/build-windows-ai-curvature logs original449b40 results and actual+cc/+b4 sub-slice chain at all three AI callsites. Awaitcapture/restorationbeforeanotheroriginalrun. Neednativecurvaturetraceat12461 andcomparephysicalcentres/bends/forwardlinks. No liveportoraclecurrently; priorheadlessjobsallfailedasrecorded.


## Retained rider physical half and terrain (continued)

Original matched-curvature capture87263 completed12462ticks, SHA c9a4fff5785fd8cb310c9258058a0ec1fca74d40ebd33c75247ccf3970c8758f. At12461 right physical half continues past flattened route merge: global1430 is physical half1 section5 local368, not logical commonsection6 local0. Physical curvature chain fix advanced strict immutable wreck through12518 (build/physical-curvature-chain-medly-wreck.json); first12519 rider2 lateral original94937/native94227.

Rider, HalfMirror and CrashActor now retain optional slice_half/previous_slice_half, copied with cached sample pointers. cross_sample follows ring forward link and keeps half until a physical common slot. accumulate/curvature/AI lookahead/vertical rise, rank and scan masks consume that half. Detached crash copies preserve it. State hash includes these fields. Reverse crossing still derives logical half: this remains to be recovered/validated, not claimed complete. General common-to-dual branch selection is also still the older split-event test; don't call the whole link graph complete.

build/retained-rider-half-medly-wreck.json advances lateral to exact94937 at12519, but first now height original2048/native0; velocity original1536/native1714. Full terrain metadata retained per built physical half in TrackRing and used by vertical and edges. build/retained-rider-terrain-medly-wreck.json still has same height difference12519; source terrain correction did NOT resolve it. Detailed native output build/retained-ground-tick12519.log: ground0, road_move8570, correct physical section5 local368, next369, centre250. Need compare original ground/rise before concluding cause.

Synthetic unequal-arm regression now passes build/physical-merge-terrain-test.log. Initial fixture lacked start sentinel causing Missing route node; repaired. Physical short-arm endpoint in this synthetic course is local383 (not Medly's368); asserts actual retained terrain independent of logical common node, forward half retention, snapshot survival. Last track_ring focused gate1/1pass0skip. Snapshot tracker serialization now prefixes map entry count.

Capture56043 was started with wrong diagnostic env RR_ORACLE_SLICE (actual is RR_ORACLE_SLICE_SAMPLE), interrupted via tool Ctrl-C; that interrupt killed Python without finally cleanup. Verified no ROADRASH remained, explicitly restored AWEMAN from artifacts/oracle-live-backups/ba754c814026439387a77d3fad0ebae2 after checking both hashes, restored/removes helper according backup, receipt phase restored-after-tool-interrupt. Verified working hashca7f2f...47022. Explicitly reaped dead own resource token3db8665f-e1fe-4996-88dc-81dc1f677413 PID3092 afteraliveFalse. Don't repeat tool interrupt for diagnostic typo; let wrapper finish normally.

Capture54774 is ACTIVE corrected medly-wreck-matched-ground-detail-20261113, original exactE:/RR PID14308, Python23640, helperoracle/build-windows-ai-curvature/AWEMAN32.DLL,12520frames, AI_TARGET12519 SLICE_SAMPLE1430, same matched startup speed18values and originalINPUT,seed1. It will dump physical ground near/far and road_move after tick12519. Wait for wrapper completion/restoration before next original run. Logbuild/medly-wreck-matched-ground-detail-capture.log. No other original process owned. Focused race component job31914 running build/retained-rider-race-test.log. No full gates/commit/release20 yet; latest public stays1.1.19.


## Ground probe resolved: billboard contact, not interpolation

Capture54774 COMPLETE0:12520ticks SHA06e6ccfe0fb5183d00190a12d9c4b77ddc96c91a53396c1bcc58972dad337ddb. WorkingAWEMAN restored and verifiedca7f2f...47022; no original active. Original12519 ground0/road_move8570 exactly equalnative. It retains billboardselector70,type1,lateral420,flags7,onebox inphysicalhalf1. Nativecachedlogicalroute1 sample1430 hadbillboards0. Height2048/velocity1536 arise afterthat contact, notwrongterrain. The old/new12519 RNG,position,height,velocity,contactground exactverified. Do notcontinueclaimingterraininterpolationcausesheightdifference.

statics_[2] now index actualphysicalhalf withring: ring_statics writes in.physical_half includingsecondarypastlogicalmerge, common0doesnoterasehalf1; sweeps and diagnostics use actorcachedhalf. Visibility candidate reads ringretained_sample. New race regression injectssecondarybillboardthencommon0 atsample365pastlogicalmerge and assertssecondarysurvives. Strict replay2530 ACTIVE build/physical-scenery-half-medly-wreck.json; focusedrace66510ACTIVE build/physical-scenery-half-race-test.log. Sourcecompilationphysical-scenery-half-build.log succeeded.

Expanded retained RouteSample data initially exposed Windows stack exhaustion in race fixture during ring setup; shoulders_={} created a full temporary. Replaced withstd::fill(singleShoulders{}), focusedrace nowPASS (build/retained-rider-race-test.log,6.48s). Temporary checkpoint diagnostics fully removed. Fullgate stillpending; no release20 claim.


## Strict wreck now reaches terminal result timing

Physical scenery replay2530 COMPLETE1, all12792ticks exact, next12793 car signal. Prior verifier text incorrectly impliedmissingfixtureidentity; originalHASsignal_position362832,index0,phase2,accum136; nativecar.signalabsent. Improved error specificallydistinguishesnativeabsence frommissingidentity. Traffic latch usedlogicalmasks whileoriginal449d30 usesphysicalsame-road gate. Patchedcar/signalphysicalmaskqueries; focusedtrafficregression proveslogicaloppositehalfcanlatchafterphysicalcommonreuse. build/physical-signal-mask-test.log1/1PASS.

Replay28769 COMPLETE1 build/physical-signal-mask-medly-wreck.json passes12963ticks;12964 rawresultnative5/original1. Allothercomparedriderfieldsremainexact at12964(build/medly-wreck-outcome-tick12964.log). Nativebody370145 aheadofbike368849 by1296; animation37loop1. recover_crash_actor incorrectlycomputed(negativeDifference XOR1)-1 becausepriorcommentclaimedlogicalshift. ExactoriginalworkingEXE449c20bytes2bd1 8bc2 99 33c2 2bc2 c3 proveCDQsignextensionandordinarysignedABS, notlogicalshift. Corrected32bitabswithINT_MINwrap; mustwaitabsolute<80 beforewreck. Testbothdirections79/80/1296withplayerloop1destroyedbike. build/wreck-absolute-distance-test.log PASS (crashcorpusplusboundaries).

FocusedracewithnewphysicalscenerytestPASS build/physical-scenery-half-race-test.log6.06s aftermovinglargeTrackRingfixturetoheap; teststackwasstillnearWindows1MBafteraddinganotherRider. Runtimefixstd::fillremainsvalid, eliminateslargefullringtemporary. NoSIGSEGVtestskip.

Replay6383 ACTIVE build/wreck-absolute-distance-medly.json, includesphysicalsignalfixandsignedABS. Buildwreck-absolute-distance-build.log succeeded. Needawaitstrict13271 andthenfreshallfive/tenpolice/fullnativegates. No originalgameactive; groundcapturecompleteandDLLrestored. No release20/commitclaimed.


## Physical node identity for finish standings

Replay6383 COMPLETE1 build/wreck-absolute-distance-medly.json passes13086, next13087 rider10raw1f8 original99345/native99318. Originalmoves366029→366090(61), finishdistance99406→99345: samephysicalnode, subtract61. Nativephysical_node lambda stillusedlogicalsample.half, crossing1430 falselyswitchednode124→188 andresetgraphdistance. Correctedlambda usesr.slice_half andretained_sample; parkingforwardclosed gatealso usescachedhalf.

An overbroad simultaneousremaining_finish_distance changeusedr.sampleforqueryevenwhencallerpassedhalfposition. Itregressed969by256 (build/physical-finish-node-medly-wreck.json); revertedthatargumenttooriginalpositionwhilekeepingcachedphysicalhalf. Don'tclaimfailedcombinedversionaccepted. Replay80306 ACTIVE build/physical-finish-node-position-medly.json, buildphysical-finish-node-position-build.logsuccess. AlloriginalprocessesclosedandworkingDLLrestored; noactiveresourceclaims. No pendingcompilationexceptheadless80306; don'trelinkwhileitloadsoracle-simexe.


## Complete immutable wreck acceptance; full gates active

Replay80306 COMPLETE1, passes13270 thenfinal13271 ring.front original389333/native389336. Rawregisteredwrecknowcorrect13271. Original416066..41606a startsDAT4753c4 resultdelay forpositive durationincludingwreck, notonlyarrest. Nativeupdate_camera rampcheckedonlyfinishbit/busted; nowalsoanyregisteredresult. ComponentracecameraassertPASS build/wreck-result-camera-test.log.

Replay12017 COMPLETE0 build/wreck-result-camera-medly.json: strictall13271ticksidenticalincludingfinalrawoutcome5andringcamera. ImmutablefixtureSHAfff54173...same, nohydration/reanchor. Thisiswreckacceptance, NOTfinishedrace.

Fullbuild97275 COMPLETE0 build/v1120-full-build.log. Currentheadlessgates(no original desktopprocess): native70348 build/v1120-native-tests.log/XML153selected, onlyownerwideexcluded, stdoutcap1MB toauditskips; fivewhole45326 build/v1120-whole-race-acceptance.json/log; HiwayL5long45922 build/v1120-release-regressions.json/log94929througharrest. Allthreeusecurrentoracle-simexe: DO NOTrelinkuntiltheyfinish. Sourceafterfullbuildonlydocupdates; race.cppcameraCommentupdatedBEFOREfullbuild. Needinspecteachresult, fixfailures, auditinternalmarkers. Lastnativeobserved20/153pass; nofailedobservedyet. Additionalnativeoracle-wreckcase12passed.

Release20scripts PREPAREDbutNEVEREXECUTED: New project/verify-parity-v1120-upgrade.py(19→20,noassetargs),publish-v1120.py requiresnewnativeSummary(required_police_cases_passed10),fivefull,L5,wreck,cleancommit/remote/upgradeguards. Nofunctionalcommit/push/release20yet; publicstill19. Needfinishgates,reviewexplicitpaths,commitshort/pushthenfreshcleanprovenancebuild/package/upgradeverification/publication. NewdocsPROGRESS/VERIFICATIONrecordacceptedwreckonlyandpendingbroadergates. Noagents/CUAactive.


## First complete gate and snapshot allocation fix

Allinitialjobscomplete: five45326PASS5/5,total143764ticks;L5long45922PASS94929ticks througharrest4; native70348COMPLETE8 with152pass/1fail/0CTestskip/153selected431.32s. AlltenpolicecasesPASS. Preservedfailedgateasbuild/v1120-native-attempt1.log/XML. Onlyfailure race_state: warmed snapshotcopyallocatesone tracker-mapnode. No othernativefailure/skips waived.

TrackerRefs map nowsharedcopy-on-write (likeotherexistingringsnapshotbuffers). setupcreatesfreshmap; step_trackers takesmutableuniquecopybeforewrites; readqueries/statehashdereferenceconstsnapshot. DefaultTrackRingcopysharesmapandallocatesnothing. FirstcompileattemptusedremovedC++20shared_ptr::unique; correctedtouse_count()!=1. Focused14100 COMPLETE0 build/tracker-cow-snapshot-test.logPASS5.83s, coversallocationfreewarmedcopies/restoration/splits/twohumanwindows. Buildtracker-cow-snapshot-build.logsuccess. Noactiveruns. Needfunctionalcommitandcleanfullbuild/gatesbeforeinstaller20; all5/L5pre-COWreportsvalidfortheirtestedbinaryc1a849b4feaf42ef2827d59a171f6bc84ba4d5c1868b1d8a3bb42b417891e7ca, notautomaticallyproofnewbinary.

Useraskedwhyworkcontinuesif5racespass. AnsweredextraMedlywreckexposeduncoveredendingbugs(nowall13271pass), currentworkonlyintroducedsnapshotallocationregressionandcommit/push/installerrelease. No neworiginalcapturesneeded/running. Continuefinishingthisbatchwithoutinventingextraacceptanceobjectives.
