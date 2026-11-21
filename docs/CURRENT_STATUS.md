# Current status and evidence

## November 21, 2026 - visible cliff-fall correction

The CANYN level-5 edge-crossing regression reproduces the disappearance without
an original-game capture: a fatal cliff step changes physical ground from road
height to hundreds of units below it and resets relative height. Using that pose
directly projects the bike/body below the viewport immediately. The original
0043a50b..0043a532 reset exists too; it was not removed from simulation.

Presentation now retains a continuous vertical fall across that reset, using
the actor's vertical velocity and gravity at 60 Hz. Bike and detached-body poses
are tracked separately; shadows retain physical ground. Game::rider_nodes now
uses the vertical component of the captured pose. Physics, contacts, wreck timing
and gameplay RNG remain untouched. This is a presentation correction, not a claim
of measured original post-cliff rendering parity.

The real CANYN level-5 outer edge is crossed with actual accelerate/turn inputs
in both biker-art modes. Disabling the correction (--cliff-raw) fails the entry
continuity assertion; enabling it keeps the player visible for the first 12 fall
frames and verifies continued downward motion over 90 ticks. All six level-5
CTest gates pass, including the retained 712-tick native scene comparison and
65 original-list raster frames with zero pixel differences. Logs:
build/level5-cliff-raw-failure-20261121.log,
build/level5-cliff-continuity-20261121.log,
build/level5-cliff-final-gates-20261121.log.

The Release rr-game executable also builds successfully; focused bob and
view-state component tests pass (2/2). Logs: build/cliff-game-build-20261121.log
and build/cliff-components-20261121.log.

No game window, original capture or installer execution was used. The separate
reported upright-pedestrian failure remains unresolved; its controlled native
checks passing do not contradict the report.

**Windows race rendering, November 20:**42 focused regressions pass without skips.
Native camera/world construction matches52 Napa and35 Highway sampled composed
scene lists through finished recordings;87 original-list raster checks have zero
pixel differences. Remaining complete-course/level/frame-history coverage is
explicitly open. [Current evidence and capture constraint](agent-handoffs/native-camera-parity-20261120.md).

Generated from `agent-status.json`. Historical entries describe their own checkpoints.

| Area | Implementation | Verification / remaining acceptance | Evidence |
| --- | --- | --- | --- |
| Native build | Playable native game + research viewer | Windows optimized MSVC Release:152 selected CTests pass, zero failures/CTest skips; owner excludes menu pixel comparisons and widescreen-only checks. Actual compiler configuration now recorded in build metadata. Historical macOS/Linux gates retain their own dates.; Release1.1.19 published and public checksum verified;1.1.18->1.1.19update preserves600checked files. Further simulation coverage remains separate. | [VERIFICATION.md](VERIFICATION.md), [RUN_NATIVE.md](RUN_NATIVE.md), [field-parity-20261018.md](agent-handoffs/field-parity-20261018.md) |
| Simulation parity | Original live rider/car scheduler, native countdown, scene construction, AI combat/relationships, car callbacks, marking lifetimes and single-anchor replay | Optimized five Level1 human races match143764 ticks from tick1 through finish with one RNG anchor. HighwayLevel5 matches94929ticks through arrest. Ten required police/crash cases match35916ticks. Initial-slot geometry and both normal/crashed finish-result integration verified.; Higher-level/branch finish combinations remain unmeasured. Release1.1.19publishes the verified milestone. Menu and widescreen-only parity excluded by owner. Multiplayer held. | [skip-removal-20261112.md](agent-handoffs/skip-removal-20261112.md), [parity-resource-lookup-20261112.md](agent-handoffs/parity-resource-lookup-20261112.md), [parity-city-visibility-20261112.md](agent-handoffs/parity-city-visibility-20261112.md), [parity-projected-velocity-20261112.md](agent-handoffs/parity-projected-velocity-20261112.md), [parity-mixed-contacts-20261112.md](agent-handoffs/parity-mixed-contacts-20261112.md), [parity-finish-pools-20261112.md](agent-handoffs/parity-finish-pools-20261112.md), [SIM_SCENARIOS.md](SIM_SCENARIOS.md), [10-simulation-oracle.md](plans/10-simulation-oracle.md), [VERIFICATION.md](VERIFICATION.md), [field-parity-20261018.md](agent-handoffs/field-parity-20261018.md), [remount-mirror-20261104.md](agent-handoffs/remount-mirror-20261104.md), [split-ring-latest-20261104.md](agent-handoffs/split-ring-latest-20261104.md) |
| Napa race | Native scene, riders, traffic, live police and collision scheduling | Strict completed L1/seed1 human recording29055/29055 ticks identical including RNG, single seed anchor (Windows November12); Other levels, branches, inputs and behavior outside the measured recording | [NAPA_IMPLEMENTATION.md](NAPA_IMPLEMENTATION.md), [11a-napa-playtest-fixes.md](plans/11a-napa-playtest-fixes.md), [16-gameplay-systems.md](plans/16-gameplay-systems.md) |
| Other courses | Course data supported; composed renderer work pending | Strict human full-race replays match Hiway14084, Napa29055, Canyn29620, City33819 and Medly37186 ticks including RNG from one anchor (Windows November12; Level1, Medly seed2, others seed1); Five completed L1 human recordings pass strict whole-race acceptance; other levels/branches and unexercised behavior remain unverified. | [parity-resource-lookup-20261112.md](agent-handoffs/parity-resource-lookup-20261112.md), [17-all-courses-port.md](plans/17-all-courses-port.md), [parity-finish-pools-20261112.md](agent-handoffs/parity-finish-pools-20261112.md) |
| Frontend/settings | Menus, career, saves, jukebox, sound mix and controls implemented; online mano-a-mano (plan 34) live through `rr-netd` with room codes, invite links and six-platform CI cross-play | Windows GDI memory components match9856 disc glyphs per surface,672 system glyphs per surface,196608 RGB565 blends,292 line heights,7 extents and6 wrapped rectangles. Functional menu/network-menu components pass. Historical Wine screen counts are not current Windows acceptance.; Menu pixel parity excluded by owner. Native original finished-City RRS equals its152-byte memory payload; native parser/writer byte identity and memory place/cash checks pass. | [skip-removal-20261112.md](agent-handoffs/skip-removal-20261112.md), [18-settings-and-options.md](plans/18-settings-and-options.md), [13-music-and-frontend-completion.md](plans/13-music-and-frontend-completion.md), [frontend-layout-20261026.md](agent-handoffs/frontend-layout-20261026.md), [frontend-exact-20261031.md](agent-handoffs/frontend-exact-20261031.md), [34-multiplayer.md](plans/34-multiplayer.md) |
| Android | Native asset-free APK build | Hardware validation remains unverified; Device import/render/input/lifecycle/performance | [14-android-port.md](plans/14-android-port.md), [VERIFICATION.md](VERIFICATION.md) |
| Packaging | Source/commit/config/compiler stamp and manifest check | Current package receipt below if run; Owner signing/release acceptance | [USER_GUIDE.md](USER_GUIDE.md), [15-release-and-acceptance.md](plans/15-release-and-acceptance.md) |
| Agent tooling | Canonical instructions, claims, gates, capture validation, evidence index | Tooling regression suite; receipts below; Clients must honor claims; direct unmanaged commands bypass them | [AGENT_TOOLING.md](AGENT_TOOLING.md), [REVIEW.md](agent-review/REVIEW.md) |
| Linux/Windows | Plan 33 Part A portability groundwork; 33b Windows (Tasks 1-7) and 33a Linux/Steam Deck (Tasks 1-7) done in software: Linux sniper-SDK tarball/AppImage and Flatpak, pad/keyboard import with an in-game browser and disc search, Deck/gamescope first-run defaults, Steam keyboard hint and pad name entry, suspend pause, Linux CI and release assets | Linux GCC 14 / Clang 19 on x86-64 and ARM64 (Debian trixie, Docker, disc mounted): full suite on all four, goldens equal to macOS, ASan/UBSan and TSan clean; Linux packages verified (glibc <= 2.30, libc/libm/libdl/libpthread only, no game data) and launched on clean Ubuntu 20.04, Debian 12, Fedora and Arch; X11 (Xvfb) and Wayland (Weston, scale 1 and 2) render; Flatpak built and launched headless; determinism matrix equal on Linux GCC/Clang x86-64/ARM64 (synthetic and disc); Windows CI (plan 33b): MSVC x64 and ARM64 full suite (disc tests skipped), packages verified and install-tested, unsigned; cross-play CI: six platforms (Linux x86-64/ARM64, macOS, Windows x64/ARM64, wasm32) official with equal hashes and the disc race equal; no hardware or Steam Deck runs yet; Hardware acceptance on both: desktop Linux (GNOME/KDE/gamescope, 150% scale, idle inhibitor, audio device switching, controllers, Flatpak import and play) and Steam Deck (Desktop and Game Mode, on-screen keyboard, sleep, legibility, battery benchmark, disc determinism) for 33a; real controllers, headset swap, 144 Hz and the firewall prompt for 33b; Windows signing certificate and Flathub submission (owner) | [33-linux-windows-ports.md](plans/33-linux-windows-ports.md), [33a-linux-steam-deck.md](plans/33a-linux-steam-deck.md), [33b-windows.md](plans/33b-windows.md) |

## Latest gate receipts

- **android-debug**: passed; source `c231b349b122`; commit `25daed6d1f41`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/playtest-audio-ai-loading-android/receipt.json`.
- **android-release**: passed; source `50298bc9812a`; commit `b76ffc899e20`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/p0-p1-android-release-final/receipt.json`.
- **native**: passed; source `8718e6d4f13b`; commit `992106d9ee74`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/field-parity-20261018-native/receipt.json`.
- **oracle**: failed; source `c326c2b97d5a`; commit `b76ffc899e20`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/p0-p1-strict-final/receipt.json`.
- **package**: passed; source `0f31a63359bf`; commit `b76ffc899e20`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/p0-p1-package-final/receipt.json`.
- **sanitizers**: passed; source `8718e6d4f13b`; commit `992106d9ee74`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/field-parity-20261018-sanitizers/receipt.json`.

## November 20: CANYN level-5 work (superseded by November 21 above)

Rendering/gameplay changes are pending and uncommitted in the Windows checkout.
The latest source adds original shared rider-template page behavior after tracing
four raster differences to empty shadows. It is unbuilt; do not treat earlier
passing scene/replay logs as validation of the latest tree. Original pedestrian
impact and full visible cliff-fall behavior remain open. Captures are stopped at
the owner's request. [Resumable status](agent-handoffs/canyn-level5-gameplay-20261120.md).

## November 21: upright pedestrian investigation

The reported upright impact remains unresolved; crashed-bike/half fixes are not
claimed as its solution. Focused console targets now build under the owner's
standing build authorization. Controlled upright CANYN level-5 checks cover 13
representative pedestrian families and four approach speeds through actual scene
emission, in both biker-art modes. Both new upright CTest cases pass. The three
existing level-5 gates also pass; shared-template rendering now has zero differing
pixels in all 65 retained CANYN frames. No new game windows, captures, remote
actions or installer execution. [Results, failed probes and remaining investigation](agent-handoffs/canyn-level5-gameplay-20261120.md).
