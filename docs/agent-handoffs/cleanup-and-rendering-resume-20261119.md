Current rendering continuation supersedes the unfinished scenery section below:
see native-scenery-parity-20261119.md. Seven component windows now match1976nodes;
25focused tests pass and allfive completed simulation recordings still match.
Earlier DZHR size inference below was corrected by caller0044b9c9 (fixedscale8).

# Cleanup and rendering resume — 2026-11-19

Active checkout: E:/road-rash, port/plan33b-windows. HEAD82b13a1. Release1.1.31
published at https://github.com/P0NYSLAYSTATION/road-rash/releases/tag/3do-1.1.31
installer SHA25698953e007271aa64e01f00117c14aea83389598d057561f7ab21b7a8ee51406d.
Owner explicitly reconfirmed standing permission for builds, commits, pushes and
releases throughout this conversation. No questions/new branch. No subagents.
Original game ONLY E:/RR/ROADRASH.EXE, exact working install/AWEMAN32.DLL.
Menu parity and widescreen-only issues excluded. Do not run installers.
Use numerical automated comparisons and targeted failures, never visual review
of every race frame (owner explicitly rejected that expensive approach).

## Current task: storage cleanup

Owner requests removing almost60GB of workspace clutter without losing memory
or evidence needed to continue. Stale C:/Users/eddj9/.codex/worktrees/
windows-pacing-release/road-rash held21 parity-upgrade test installations~15GiB,
stale debug/release builds~9GiB and~34GiB analysis (mostly duplicate captures).
Active E checkout/build/dependencies remain intact. Cleanup records and a byte
exact patch of current E work are in the stale worktree's
analysis/cleanup-retained-20261119/. Full source changes remain in E itself.
No user-installed game, source assets, docs or unique accepted trace is to be
deleted. Capture copies may be deduplicated by SHA256-verified NTFS hardlinks;
lossless NTFS compression preserves original paths and hashes.

## Published milestone

Seven original terrain windows match exact polygons/material/order/ground;
Napa two-branch full route sweep12268frames/50215probes has zero covered lanes.
Four original full display-list raster replays have zero differing pixels.
Five completed human simulation replays143764ticks exact including RNG.
Full native race-frame parity is NOT complete. See rendering-work-in-progress-
20261114.md for original branch heights, shared occlusion, painter order and
validated source details; its pending-release language is superseded by1.1.31.

## Uncommitted rendering work — preserve all modified E files

1. oracle/actor_record_hooks.h and actor_record_buffer_test.c:
   absolute_before/after survive internal display-list flush; selected actor
   pointer retained to disambiguate riders sharing a grid position. Synthetic
   test now undefines NDEBUG so Release assertions actually run; saturation,
   forwarding, flush and suppressed-output checks pass. Proxy logs new fields;
   oracle-scene-test --native-actors uses full indices/identity for complete lists.
2. Captures napa-natural-actors-full-20261115 (frame0:16nodes) and
   napa-l5-natural-actors-240-20261115 (sample0/60/120/180:56nodes) pass numerical
   actor component checks. The latter includes a little actual movement after
   countdown; it is short component coverage, not full native race rendering.
3. Discovered scenery rendering still used an isolated precomputed placement
   list while collision uses the live race BORR/DZHR stream. Uncommitted source
   retains RingStatics per physical ring slice; adds BORR size/mirror and DZHR
   mirror metadata; CourseScene consumes live placements without RNG calls.
   ring_scenery converts the descriptors. Snapshot schema3 now includes both
   bands (omitted from previous codec) and live static descriptors; needs focused
   round-trip checks before integration. No new multiplayer feature work.
4. oracle-scene-test --native-static-window extends ordered component comparison
   to scenery; imports original placement fields (not projected vertices).
   Original BORR: sub+60 count,+62 size,+63 selector,+64 mirroring bit0,
   +6c laterals(abs minus sub+cc). DZHR: count+3d,16-byte entries at+40:
   selector+0,mirror+1 bit0,size/type+2,absolute lateral+8. Billboard low type
   nibble is its render size, not fixed8. Production metadata uses raw BORR
   word bits4..7 and mirror0x40000000, DZHR mirror0x80/type low nibble.
5. Latest comparison STILL FAILS scenery geometry. Baseline original missing
   Canyn20/City10/Medly24/Napa6; latest descriptor correction still requires
   investigation. Do not call this complete or publish it. Current logs in
   E:/road-rash/build/*live-static3.log and live-static-descriptor-build.log.
   Earlier live-static-build.log failed for missing track_ring.hpp include;
   that was fixed. Runs of the old binary immediately after that failed build
   are not new-source evidence. Later gated build succeeds.

Next parity action after cleanup: inspect the latest original/native scenery
vertex differences numerically (start Napa6 props), trace original0044b630 vs
scenery_node projection/hotspot/size/ground/clip and resource frame selection.
Check live scenery snapshots retain metadata, then regression suite/strict five
finishes. Commit/push Windows branch and installer/release for validated progress.
No full frame rendering parity claim yet. No held capture resource known; previous
original captures restore the working DLL automatically.

## Cleanup completed 2026-11-19

Removed 21 obsolete update-test installations, obsolete C-worktree Debug/Release
builds and installer staging (23.81 GiB). Preserved per-install metadata/settings,
saves and build configuration before deletion. Verified and hardlinked 3,351
byte-identical immutable capture copies (16.33 GiB), retaining every path and the
one distinct-version file. Applied transparent lossless NTFS compression to the
remaining analysis evidence. Final Win32 file-ID and compressed-allocation scan:
11,162 paths, 7,811 distinct files, 35.05 GiB logical including shared paths,
18.73 GiB unique uncompressed content, 7.15 GiB physical file storage. Do not sum
free-drive deltas as an exact reclaimed total: concurrent/delayed free-space changes
made the compression delta inconsistent with the initial inventory. At least
40.1 GiB recovered through measured deletions and duplicate storage elimination.

All five accepted completed-race traces passed SHA256 preservation checks; the
unfinished E-source diff matches active-windows-work.patch. Current E Release
build, dependencies, installer and all source/assets remain intact. No installer
was run. Optional broader E artifact cleanup was rejected by automatic review for
risk to unique disc assets; nothing from that request executed, and those folders
were left intact. Full cleanup receipts and resume patch are in this directory.
Next rendering action remains the numerical Napa static-prop comparison above.
