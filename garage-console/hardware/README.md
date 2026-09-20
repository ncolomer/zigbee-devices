# garage-console — hardware

16-channel switch input carrier board for a Seeed Studio XIAO ESP32C6.

Each of the 16 JST XH connectors carries GND plus one dedicated input, so a
momentary or toggle switch can be wired to it with a 2-wire cable. The ESP32C6
does not have 16 spare GPIOs, so the inputs land on an I²C port expander and the
expander raises an interrupt line when any of them changes.

Layout is modelled on the [Grove Shield for Seeeduino XIAO](https://wiki.seeedstudio.com/Grove-Shield-for-Seeeduino-XIAO-embedded-battery-management-chip/)
— a XIAO carrier with connectors broken out — minus the battery management, since
this board is permanently powered from the XIAO's USB-C.

## Design decisions

**PCF8575 rather than MCP23017.** The MCP23017 was the first choice, but its
GPA7 and GPB7 pins are output-only ([datasheet DS20001952D](https://ww1.microchip.com/downloads/en/DeviceDoc/20001952C.pdf),
pin table p.11), leaving only 14 usable inputs for 16 switches. The PCF8575 has
16 true quasi-bidirectional I/Os plus an open-drain interrupt output, so all 16
channels stay uniform. No local copy of the MCP23017 datasheet is kept — it's
a rejected alternative, not a part this design uses.

**No external pull-ups on the inputs.** The PCF8575 sources IOH = 30–300 µA to
VCC on any pin written high, which is the input configuration ([PCF8575 SCPS121I](datasheets/PCF8575-TI-datasheet.pdf)
§8.3). One 100 nF cap per input filters EMI picked up on the switch cabling and
debounces the contact in hardware. The trade-off is a slow release edge —
100 nF / 30 µA ≈ 8 ms worst case — which is harmless for switches and is why no
external pull-ups are fitted.

**Powered from the XIAO's USB-C.** +3V3 for the expander comes from the XIAO's
on-module LDO (module pin 12). Nothing on this board draws meaningful current
(PCF8575 standby is 10 µA max), so there is no regulator, no battery, and no
charger. No power indicator either — the XIAO's own `LED_BUILTIN` serves that
purpose, so the board carries no LED of its own.

## Files

| Path | Contents |
|---|---|
| `garage-console.kicad_pro` | KiCad project |
| `garage-console.kicad_sch` | Schematic (A3) |
| `garage-console.kicad_pcb` | Board layout |
| `datasheets/` | Local copies of every datasheet the design relies on |
| `3dmodels/` | Project-local STEP model(s) not available in any installed library |
| `garage_console.pretty/` | Project-local footprint library — currently empty; kept registered in `fp-lib-table` rather than deregistered, see Layout |
| `garage_console.kicad_sym` | Project-local symbol library — `XIAO_HDR_A`/`XIAO_HDR_B`, the named-pin schematic symbols behind HDR1/HDR2 |

Libraries: stock KiCad symbols/footprints, plus the two project-local
libraries above (registered project-scope in `fp-lib-table`/`sym-lib-table`
under the `garage_console` nickname). Nothing on the board comes from the
[Seeed OPL KiCad library](https://github.com/Seeed-Studio/OPL_Kicad_Library)
any more — it was only ever needed for the XIAO module's footprint, which this design
dropped (see Layout), so its entry was removed from `fp-lib-table` rather
than left pointing at a machine-specific absolute path that wouldn't
resolve on anyone else's clone.

## Switch channel map

`SWn` is pulled low while the switch on `Jn` is closed.

| Connector | Net | PCF8575 pin | Port bit |
|---|---|---|---|
| J1 | SW1 | 4 (P00) | port 0, bit 0 |
| J2 | SW2 | 5 (P01) | port 0, bit 1 |
| J3 | SW3 | 6 (P02) | port 0, bit 2 |
| J4 | SW4 | 7 (P03) | port 0, bit 3 |
| J5 | SW5 | 8 (P04) | port 0, bit 4 |
| J6 | SW6 | 9 (P05) | port 0, bit 5 |
| J7 | SW7 | 10 (P06) | port 0, bit 6 |
| J8 | SW8 | 11 (P07) | port 0, bit 7 |
| J9 | SW9 | 13 (P10) | port 1, bit 0 |
| J10 | SW10 | 14 (P11) | port 1, bit 1 |
| J11 | SW11 | 15 (P12) | port 1, bit 2 |
| J12 | SW12 | 16 (P13) | port 1, bit 3 |
| J13 | SW13 | 17 (P14) | port 1, bit 4 |
| J14 | SW14 | 18 (P15) | port 1, bit 5 |
| J15 | SW15 | 19 (P16) | port 1, bit 6 |
| J16 | SW16 | 20 (P17) | port 1, bit 7 |

Every connector's pin 2 is GND.

## XIAO interface

| XIAO pin | Signal | GPIO | Notes |
|---|---|---|---|
| 3 (D2) | PCF_INT | GPIO2 | active-low, open-drain, 10 k pull-up (R3) |
| 5 (D4) | SDA | GPIO22 | 4.7 k pull-up (R1) |
| 6 (D5) | SCL | GPIO23 | 4.7 k pull-up (R2) |
| 12 | +3V3 | — | board supply, from the XIAO LDO |
| 13 | GND | — | |

D0, D1, D3, D6–D10 and VBUS are unconnected — the expander covers all 16
channels, so no direct GPIOs are needed.

## Firmware notes

I²C address is **0x20** (7-bit): A2:A1:A0 are tied to GND, and the PCF8575
address is `0100 A2 A1 A0` (SCPS121I §8.3.3).

The part has no configuration or status registers. Writing `0xFFFF` once *is*
the input configuration; after that, reads return the pin states.

```cpp
Wire.begin(22, 23);                  // SDA=GPIO22 (D4), SCL=GPIO23 (D5)

Wire.beginTransmission(0x20);        // all 16 pins to input (weak pull-up on)
Wire.write(0xFF); Wire.write(0xFF);
Wire.endTransmission();

pinMode(2, INPUT);                   // R3 already pulls PCF_INT up
attachInterrupt(2, onChange, FALLING);

// in the handler's deferred work:
Wire.requestFrom(0x20, 2);
uint8_t p0 = Wire.read();            // SW1..SW8,  bit 0 = SW1
uint8_t p1 = Wire.read();            // SW9..SW16, bit 0 = SW9
// bit low = switch closed; the read also clears INT
```

Reading the port is what clears INT, so the read must happen on every interrupt
or the line stays asserted. A change arriving during the ACK pulse can be lost
(SCPS121I §8.3.2) — poll once per second alongside the interrupt so a missed edge
self-corrects.

## BOM

| Ref | Qty | Value | Footprint | LCSC | Notes |
|---|---|---|---|---|---|
| U1 | 1 | PCF8575 | SSOP-24 0.65 mm | [C2863388](https://www.lcsc.com/product-detail/C2863388.html) | TI PCF8575DBR |
| HDR1, HDR2 | 2 | Socket, 1×7, 2.54 mm | `PinSocket_1x07_P2.54mm_Vertical` | [C22438157](https://www.lcsc.com/product-detail/C22438157.html) | hanxia HX PM2.54-1x7P ZC-Y, female, 8.5 mm socket height; carry the XIAO module's real electrical connections, see Layout |
| J1–J16 | 16 | JST XH 2-pin | B2B-XH-A vertical, 2.5 mm | [C158012](https://www.lcsc.com/product-detail/C158012.html) | cable side: XHP-2 housing + SXH-001T-P0.6 crimps |
| C1 | 1 | 100 nF X7R | 0805 | [C49678](https://www.lcsc.com/product-detail/C49678.html) | U1 decoupling |
| C2 | 1 | 10 µF X5R | 0805 | [C15850](https://www.lcsc.com/product-detail/C15850.html) | +3V3 bulk, next to the module's supply pin (HDR2) |
| C3–C18 | 16 | 100 nF X7R | 0805 | [C49678](https://www.lcsc.com/product-detail/C49678.html) | input filter, one per channel |
| R1, R2 | 2 | 4.7 kΩ | 0805 | [C17673](https://www.lcsc.com/product-detail/C17673.html) | I²C pull-ups (the XIAO has none) |
| R3 | 1 | 10 kΩ | 0805 | [C17414](https://www.lcsc.com/product-detail/C17414.html) | PCF_INT pull-up |

**The Seeed Studio XIAO ESP32C6 module itself is not in this BOM or schematic at all** — it
carries no copper on this board (see Layout) and its own pinout isn't formally
documented anywhere in this design beyond HDR1/HDR2's own pin names, so it's
easy to read the BOM and schematic and not realize a module is needed at all.
It must be sourced separately (Seeed 113991054), and its own DIP-14
through-holes need a set of male 2.54 mm pins soldered in by hand — these
plug into HDR1/HDR2's sockets, which is how the module is mounted.

No mounting holes — this board lives inside an enclosure that doesn't need them.

All passive LCSC numbers are JLCPCB Basic-library parts (no Extended-part assembly fee), picked for highest stock among matching value/footprint/tolerance. U1, J1–J16, and HDR1/HDR2 have no Basic-library equivalent — this specific I/O expander, the JST connectors, and 2.54 mm pin sockets aren't stocked there — so each lists the highest-stock Extended part matching the MPN already in the schematic.

Every part above also carries its LCSC number as an `LCSC` field on the schematic symbol itself (not just this table), so it flows straight through KiCad's own BOM export and into `kicad-jlcpcb-tools`.

Fab target: JLCPCB, 2 layer, 0.2 mm minimum trace and clearance. All passives are
0805 — easier to hand-solder/rework than 0402, at the cost of a slightly larger
board.

## Layout

The board is a 169.3 × 21.2 mm horizontal strip (trimmed to the connectors'
and module's actual extents — no unused margin, since there are no mounting
holes to clear). The top and bottom edges are colinear with the module's real
body (its footprint's `F.Courtyard`/`F.Fab` outline, back when it still had a
dedicated footprint — not the USB-C connector's silkscreen mark, which is
meant to hang off the edge — see below), not just "close to it". The left and
right edges are mirrored around the connector block's own centerline
(x = 93.15 mm) rather than the board's raw geometric center, which an earlier
revision's asymmetric width trim had drifted 0.75 mm away from. The 16 JST
connectors form two rows of 8 (one row per board edge), split into two
4-connector blocks per row with U1/HDR1/HDR2 sitting in the gap between the
blocks, centered both horizontally and vertically. The module's ceramic
antenna faces the top edge (right at the board edge — see antenna clearance
note below), USB-C faces the bottom edge. Each filter cap (C3–C18) is rotated
90° from a naive placement so its short axis (1.25 mm) faces the tight
left-right direction between connector columns, trading it for the board's
more plentiful vertical room.

**Filter caps are genuinely mirror-symmetric block-to-block.** Every JST
footprint (rotation 0° in both blocks) extends 5.475 mm to its own right and
only 2.975 mm to its left — an asymmetry baked into the footprint itself that
doesn't flip between blocks, so a true mirror needs an *asymmetric* offset
magnitude, not just an opposite sign. Left-block caps (C3–C10) sit at
`connector_x + 7.75 mm` (the connector's long side, verified clear of its
own courtyard with margin); most right-block caps (C12–C14, C16–C18) sit at
`connector_x − 5.3 mm` — the connector's short side, which needs less
clearance than the long side does. **C11 and C15 are the two exceptions**:
both sit at `connector_x + 7.805 mm`, on the same side as the left-block
caps rather than mirrored, because the mirrored position would put them
inside the R1–R3 group's courtyards (R1–R3 are fixed, off-limits to move).

**The XIAO module has no schematic symbol and no PCB footprint at all.** Its
real electrical connections are carried entirely by two standard 1×7,
2.54 mm female sockets, HDR1 and HDR2 — one per physical pin column of the
module's DIP-14 footprint (HDR1: GPIO0/GPIO1/PCF_INT/GPIO21/SDA/SCL/GPIO16;
HDR2: GPIO17/GPIO19/GPIO20/GPIO18/+3V3/GND/VBUS), each with correctly-named
pins in the schematic. HDR1/HDR2 are soldered flat onto this board; the
module's own male pins (soldered into its DIP-14 holes by hand, off-board —
not a part in this BOM) plug into the sockets, physically elevating the
module above the board surface by the socket height so it clears U1 and C1
underneath without touching them. This keeps every component on one side of
the board, which JLCPCB only charges a single assembly fee for (a 2-sided
assembly would double that fee for the sake of two parts). With no PCB
footprint for the module at all, there's nothing for the `courtyards_overlap`
DRC check to compare against U1/C1's courtyards in the first place.

An earlier revision instead gave the module its own dedicated PCB footprint
— zero pads, no courtyard, existing purely to hold the 3D model — while
keeping its schematic symbol fully wired for documentation. That worked, but
KiCad's schematic-parity check expects every schematic pin to find a
correspondingly-numbered footprint pad, and a zero-pad footprint can never
satisfy that: it produced a structural, permanent warning for all 14 pins
("no pad found for pin N in schematic"), one that could only be silenced
with a DRC exclusion, not actually resolved. Since 3D models are attached to
footprints and never inspected by DRC/ERC at all, the cleaner fix was to drop
that placeholder symbol/footprint entirely and instead give HDR1's own real,
already-connected footprint a *second*, independent 3D model entry — the
module's STEP file, positioned via its own offset/rotation so it renders in
the same physical spot as before. No symbol means no parity check to fail;
the module still renders correctly in the 3D viewer since a footprint (HDR1's)
is still there to carry it. The final transform is offset (13.73, −9.36,
8.8 mm), rotation (−90°, 0°, −90°).

**HDR1/HDR2's spacing was wrong for a while, and so was the transform above
until it was corrected alongside it.** An earlier revision placed HDR1 and
HDR2 16.4925 mm apart — the real, authoritative spacing (confirmed from
Seeed's own official OPL KiCad library footprint,
`Seeed Studio XIAO Series Library/XIAO-ESP32-C6-DIP.kicad_mod`, the same
library this project's schematic uses) is 15.24 mm, centered on x = 93.15 mm.
The footprint defines each pin twice — a real `thru_hole circle` pad and a
cosmetic `smd roundrect` annular pad 0.835 mm further out — and the original
extraction picked up the cosmetic pad for HDR2 and an incorrect average of
both pads for HDR1, instead of the real through-holes alone. Corrected
positions: HDR1 x = 100.77 (was 101.1875), HDR2 x = 85.53 (was 84.695), Y and
rotation unchanged for both. The module's 3D-model offset above is the
result *after* this correction — HDR1 moving −0.4175 mm required the same
shift in the model's offset.x to keep the module's world-space position
exactly where it had been visually confirmed correct. Re-verified with
`get_board_2d_view` (not `kicad-cli pcb render` — the two don't visually
agree closely enough for fine placement checks, confirmed directly): the
module now sits flush against both header columns with no gap on either
side. The one caveat on the model itself is unchanged: this second model is
a board-instance-local addition, not part of HDR1's stock library footprint
(`Connector_PinSocket_2.54mm:PinSocket_1x07_P2.54mm_Vertical`) — running
"Update Footprint from Library" on HDR1 would silently drop it.

Swapping the module's oversized, module-specific pads for HDR1/HDR2's small
standard socket pads also freed real copper/routing room in what was the
densest spot on the board — those oversized pads were a direct cause of
several clearance fights during autorouting in earlier revisions.

**Antenna clearance.** No keepout zone under the module's antenna end — with the
antenna now at the board's physical edge (copper on one side only, not
surrounding it) and the module elevated a few mm above the board surface on
the headers, the RF justification for a precautionary carve-out is much
weaker than when the module sat flush on the surface. Removed rather than
carried forward as unnecessary caution.

**Ground plane.** Both copper layers carry a full-board GND pour with a
*solid* pad connection (no thermal-relief spokes) rather than KiCad's default
thermal mode. The PCF8575's 0.65 mm pin pitch is too tight for the standard
2-spoke thermal relief — it kept failing DRC's `starved_thermal` check — and
solid fill also avoids the connectivity islands thermal relief can leave
behind on a densely-routed 2-layer board. The pour outline sits 0.2 mm inside
the board edge on all sides — JLCPCB's published minimum copper-to-routed-edge
clearance — rather than the tighter, sub-minimum inset an earlier revision
used. No dedicated GND stitching vias are needed — the 16 JST connectors'
and both header sockets' through-hole pins already bridge F.Cu and B.Cu
everywhere the dense signal routing would otherwise leave a pour region
isolated; each fill fragment on both layers was checked directly against
the via/pad list and every one lands on a real GND connection.

One `unconnected_items` DRC finding is left and is a known, checked-benign
zone-fill artifact, not a real gap: at the top-right corner (the rounded
corner's zone-inset vertex, board edge minus the 3 mm corner radius) DRC
reports `GND_top_solid` and `GND_bottom_solid` as unconnected. Direct
polygon inspection shows both layers' fill share the exact same points at
that vertex, and both zones tie into the same net through dozens of shared
GND pads elsewhere on the board — a stitching via added right at the flagged
point didn't change the finding (and cost real edge clearance), confirming
it's a KiCad connectivity-check quirk specific to that corner geometry, not
an electrical break. This has now shown up at the same relative corner
across more than one revision of this outline.

**Routing.** Autorouted with [Freerouting](https://github.com/freerouting/freerouting)
via its MCP server, driven from a Specctra DSN exported through KiCad's own
`pcbnew.ExportSpecctraDSN`/`ImportSpecctraSES` (the same mechanism as *File →
Export/Import → Specctra*, just scripted). The GND pours export as Specctra
`plane` records **as long as they exist at export time** — Freerouting then
treats GND as already satisfied by the plane and never draws an explicit GND
trace or via for it, so the 16 filter caps' GND legs, and everything else on
the net, rely purely on the pour (see Ground Plane above). This is
different from the antenna keepout (removed in this revision) and any other
rule-area keepout zone, which Freerouting treats as a hard routing obstacle
rather than just a copper-fill exclusion — a keepout can only exist once
routing is done, while a GND *pour* zone should stay in place through export
precisely so Freerouting reads it as a plane instead of routing GND as
ordinary traces.
A few of Freerouting's automatic-neckdown segments came back under the 0.2 mm
fab minimum and were widened by hand after import; a couple of vias needed
small nudges to clear adjacent copper. One or two connections per revision
have consistently been too tightly boxed in for Freerouting to close on its
own — U1's SW-net pins sit on a 0.65 mm pitch with GND/SDA/SCL fan-out
immediately around them, and different specific connections have hit this
depending on the exact routing pass (this revision: `SW11` between C13 and
U1 pin 15, plus a trivial direct `+3V3` link between R1 and R2 that
Freerouting simply missed). Each SW-net case was solved with a small
grid-based pathfinder (clearance-aware, both copper layers, via cost
included) rather than by guesswork, since the pocket is tight enough that
hand-picked straight lines and even a first pathfinder pass (modeling only
trace clearance, not the larger clearance a via itself needs) reliably
landed on or too close to existing copper — the working version models via
placement with its own, stricter clearance check before committing to a
layer-switch point. Every change was re-verified with a fresh DRC pass.

**3D models.** J1–J16, U1, HDR1 and HDR2 use the standard `.step` models
bundled with their KiCad libraries. The OPL library's XIAO footprints have no
dedicated XIAO ESP32C6 model — only placeholder bodies for other XIAO
variants, gated behind a `${AMZPATH}` environment variable this machine
doesn't have configured — so `3dmodels/seeed-studio-xiao-esp32c6.step`
(copied from the `water-tank-monitor` project's assets) is used instead, via
a `${KIPRJMOD}`-relative path, attached as HDR1's second 3D model entry (see
Layout for why and the exact transform).
