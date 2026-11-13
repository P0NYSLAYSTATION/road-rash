# Correct full and small HUD geometry — 2026-11-13

The supplied screenshots exposed a regression in 1.1.24: enabling Hud::set_low_mode
activated a path mixing native 320x240 art, halved high-resolution coordinates,
and a 640x480 destination. Text/needles/meters moved into the upper-left and
artwork was placed too low. Mini composition also returned after speed only.
The preceding frame-difference check established a mode change, not correct HUD
geometry or field completeness. Its success did not establish a correct HUD.

Read-only disassembly of the working E:/RR/ROADRASH.EXE confirms:
- 00446c48..00446c64 selects (MiniDash + HiRes*2)*0xdc at 00474f50.
  All four layouts must be read; low coordinates are not scaled high coordinates.
- 00446c83..00446cf4 draws the first two meters in full and mini, and the third
  for full or a nonempty target. 00446cf9 gates only the dashboard artwork.
- 00446d9f..0044706f draws distance, place, player and target/gap in both modes.
  Only 0044706f..00447284 branches numeric speed versus needles.
- 0044ca66..0044cb74 selects native LILMETER source rectangles: horizontal
  background {31,8,64,18}, fill {31,0,31+value,8}; vertical {6,0,31,15} with
  empty mask {6,16,31,31-(value>>1)}. High rectangles remain separate.
- 00442a6b..00442b24 selects Badloc 32/18 and Wingdings 18/12 for high/low.
  Gap arrow low offsets are -9,+3/+5; high -12,+9/+11.

Hud now composes the appropriate native layout entirely in its own coordinates,
including native artwork, meters, text and needle widths. Low mode doubles once
at the compositor interface. Small mode retains all shared fields. This
supersedes the older numeric-only mini conclusion and the tests that pinned
half-positioned low needles. No original fixture/golden was modified.

Evidence: build/original-hud-{draw,meter,fonts}.asm; hud-layout-test.log PASS;
hud-layout-screen-test.log PASS through the real renderer/F10 mode cycle.
New assertions check low artwork pixels at 240-artwork_height, original needle
position and doubled band, bottom-only placement, both mini player meters,
opponent meter and place/distance updates. Local generated low/high full/mini
previews (build/hud-layout-*.png) were visually inspected for the low modes
against the supplied screenshots. No original capture/replay or installer test
was run. Simulation behavior is unchanged by this presentation correction.
