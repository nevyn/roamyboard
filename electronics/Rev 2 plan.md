> 💡 Execution plan for key module rev 2, written 2026-09-16 so it can be carried out in a fresh session. Background in [[Build Diary]] ("The switch footprint is mirrored") and geometry in [[Electronics]].

## What is wrong in rev 1

The kbd library footprint `keyswitch_choc12_hotswap_1u` has the Choc contact holes at (0, 5.9) and (+5, 3.7) in footprint coordinates. A Choc v1 inserted from the front needs them at (0, 5.95) and (-5, 3.75): Kailh PG1350 drawing (bottom view and "P.C.B Layout, Pattern Side") and daprice's `Kailh_socket_PG1350` footprint agree. Rev 1 boards (JLCPCB order W2026091601072790, 20 pcs) cannot take switches. Everything else on them is right and they still validate connectors, bent pins, the 165 chain and case fit.

## The fix, in one sentence

The correct pattern rotated 180° is the rev 1 pattern mirrored top-to-bottom, so: keep the outline and the +3.3V pad on the right edge, put the hotswap socket **above** each key instead of below, move the three connector rows and U1 up into the gaps that move with it, reroute.

## Footprint

Vendored copy of daprice's footprint (github.com/daprice/keyswitches.pretty; the repo root had no LICENSE file when fetched, so redraw from the Kailh numbers in the table below if licensing matters) is at `KeyModule/Library.pretty/Kailh_choc_v1_hotswap_daprice.kicad_mod` (KiCad 5 syntax with `tstamp`; KiCad 10 loads it). Its pads, footprint coordinates, y down:

| Feature | Position | Size |
| --- | --- | --- |
| Contact hole A | (0, 5.95) | 3.0 NPTH |
| Contact hole B | (-5, 3.75) | 3.0 NPTH |
| Socket pad 1 | (3.275, 5.95) | 2.6 × 2.6 SMD on B.Cu |
| Socket pad 2 | (-8.275, 3.75) | 2.6 × 2.6 SMD on B.Cu |
| Centre boss | (0, 0) | 3.429 NPTH |
| Locating pins | (±5.5, 0) | 1.7018 NPTH |
| Third pin | (5.22, -4.2) | 0.99 NPTH |

Place every SW at **rotation 180°** on F.Cu at the existing key centres (x = 77.5; y = 51.93, 70.93, 89.86, 108.93, 128.0). At 180° the board-frame positions per key are: hole A (77.5, y-5.95), hole B (82.5, y-3.75), pad 1 (74.225, y-5.95), pad 2 (85.775, y-3.75), third pin (72.28, y+4.2).

Net mapping: the schematic's SW_Push pin 1 is +3.3V and pin 2 is the key net. Make the **right-edge pad (daprice pad 2, at x 85.775) carry +3.3V**: either swap pad numbers in the vendored footprint (pad "1" ↔ "2") or swap the two nets on each SW symbol. Swapping pad numbers in the footprint is one edit and keeps the schematic untouched; do that and note it in the footprint's descr.

Add what daprice lacks, as in rev 1: F.CrtYd 14 × 14 square (switch body), B.CrtYd covering the socket and pads (x -9.5..4.5, y 1.2..8.25 in footprint coords, which at 180° lands at x 73..87, y-8.25..y-1.2) merged with the boss circle r 2.6 and locating-pin circles r 1.15, no overlaps or DRC calls it malformed. 3D models: the original `kailh_choc.step` with rotate (0 0 0), NOT the mirrored one (reverted); the socket `kailh_choc_socket.step` with rotate (0 180 0) offset z -1.6 as before but check in a render that the cups face the board.

**Chirality proof before routing anything**: slice the original `kailh_choc.step` 1.9 mm below its base (script exists in this session's history; CadQuery `section()` at zmin+1.0) → pins at 3D (0, 5.9) and (5, 3.8). Footprint holes at 180° placement are (0, -5.95) and (5, -3.75) in board y-down, which is (0, 5.95) and (5, 3.75) in 3D y-up. They must coincide. If they don't, stop.

## Board edge

Pad 2 at x 85.775 ± 1.3 reaches 87.075; rev 1's right edge is 87.05 and copper-to-edge is 0.3. Move the right edge to **87.4** (board 16.85 mm wide) or shift keys to x = 77.15. Prefer the edge move; then header origin = 87.4 - 4.1 = 83.3, and Electronics.md's table gets the new width.

## Placement

Back-side free bands now sit between a key's locating pins (y + 1.0) and the next key's socket courtyard (y_next - 8.25):

| Band | y range | Goes there |
| --- | --- | --- |
| SW5/SW4 | 52.9 .. 62.7 | J_LEFT1 / J_RIGHT1 at y = **57.8** |
| SW4/SW3 | 71.9 .. 81.6 | J_LEFT2 / J_RIGHT2 at y = **76.8** |
| SW3/SW2 | 90.8 .. 100.7 | U1 at (81.33, **95.75**) rot -90 |
| SW2/SW1 | 109.9 .. 119.8 | J_LEFT3 / J_RIGHT3 at y = **114.9** |

Connector x stays: sockets 79.70, headers 83.3 (after the edge move). Row bodies are 7.87 tall; check ≥ 0.2 mm to the locating-pin keepouts above and the socket courtyards below. Front-side passives can stay where they are (front gaps between switch bodies do not move): C1 (76.25, 99.4), R2 (82.95, 99.4), R3 (79.6, 99.9) rot 90, R4 (72.3, 81) rot 90, R5 (83.6, 60) rot 180, R1 (76, 118.4). Re-check each against the moved rows' pads and the +3.3V route.

## Routing

Same nets as rev 1 (see Electronics.md pinout table: row 2 is CLK / PL / DATA). Suggested changes from rev 1:

- +3.3V can now run on the **back** along x ≈ 86.35 straight through every pad 2 (same net), passing header bodies (plastic) and clear of header pads (≤ 84.9). No per-key vias. Feed U1 pin 16 and C1 from it as before.
- Key nets leave pad 1 at (74.225, y-5.95) on the back; the left front lanes L1 (74.45) and L2 (73.75) are still available but check vias against the new pad 1 x-range 72.9..75.5.
- Everything else: re-derive from the rev 1 recipe; keep 0.25 tracks, 0.7/0.3 vias, one GND stitching via inside every fill island the tracks cut off.
- DATA on row 2 pin 3 is unchanged and still the only pin whose track can leave without crossing a pass-through.

## Tooling that worked, and traps

- Edit the .kicad_pcb / .kicad_sch as text; Nevyn presses File → Revert in KiCad afterwards. The IPC API cannot flip footprints in KiCad 10.0.0 and its flip action produces id-less items that duplicate on every later update.
- Nets are by name in this file format: `(net "GND")` on pads, segments, vias.
- **Strip every `(filled_polygon ...)` block from the zone before running DRC after adding vias**, or the loader assigns GND to any new via sitting in stale fill.
- DRC: `kicad-cli pcb drc --severity-all --schematic-parity --refill-zones --save-board --format json`. ERC: `kicad-cli sch erc`. Both binaries under /Applications/KiCad/KiCad.app/Contents/MacOS/. Fab: `pcb export gerbers` (F/B Cu, Paste, SilkS, Mask, Edge.Cuts, --no-protel-ext --subtract-soldermask) + `pcb export drill` (excellon, separate TH, gerberx2 maps) → zip into `KeyModule/fab/`.
- Renders: `kicad-cli pcb render`; ${KIPRJMOD} and ${KICAD_KBD_DIR} do not resolve in a /tmp copy, sed them to absolute paths first. Rotated 0805s need the pad `(at x y ROT)` angle set or DRC reports a library mismatch.
- Board pull-downs are on the front; the switch housing sits flat, so nothing may sit inside the 14 × 14 body square on the front.

## After routing

1. DRC clean, parity clean, ERC clean; render top and bottom; look at the switch model pins against the socket cups from below.
2. Regenerate `fab/KeyModule-gerbers.zip` and `fab/KeyModule-bom.csv` (BOM is unchanged: same sockets, same parts).
3. Electronics.md: board width, connector rows, "socket above each key"; the module-joint drawing is unaffected.
4. Order at JLCPCB with "confirm production file" on; verify the upload is byte-identical to the repo zip apart from timestamps before saving to cart.
