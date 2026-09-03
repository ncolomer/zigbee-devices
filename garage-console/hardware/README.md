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
GPA7 and GPB7 pins are output-only ([datasheet DS20001952D](datasheets/MCP23017-DS20001952.pdf),
pin table p.11), leaving only 14 usable inputs for 16 switches. The PCF8575 has
16 true quasi-bidirectional I/Os plus an open-drain interrupt output, so all 16
channels stay uniform. The MCP23017 datasheet is kept here as the record of why.

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

Libraries: stock KiCad symbols/footprints, plus `Seeed_Studio_XIAO_Series` from
the [Seeed OPL KiCad library](https://github.com/Seeed-Studio/OPL_Kicad_Library)
(registered project-scope in `fp-lib-table`).

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
| U1 | 1 | XIAO ESP32C6 | XIAO-ESP32-C6-DIP | — | Seeed 113991054, socketed or soldered |
| U2 | 1 | PCF8575 | SSOP-24 0.65 mm | [C2863388](https://www.lcsc.com/product-detail/C2863388.html) | TI PCF8575DBR |
| J1–J16 | 16 | JST XH 2-pin | B2B-XH-A vertical, 2.5 mm | [C158012](https://www.lcsc.com/product-detail/C158012.html) | cable side: XHP-2 housing + SXH-001T-P0.6 crimps |
| C1 | 1 | 100 nF X7R | 0805 | [C49678](https://www.lcsc.com/product-detail/C49678.html) | U2 decoupling |
| C2 | 1 | 10 µF X5R | 0805 | [C15850](https://www.lcsc.com/product-detail/C15850.html) | +3V3 bulk, next to U1's supply pin |
| C3–C18 | 16 | 100 nF X7R | 0805 | [C49678](https://www.lcsc.com/product-detail/C49678.html) | input filter, one per channel |
| R1, R2 | 2 | 4.7 kΩ | 0805 | [C17673](https://www.lcsc.com/product-detail/C17673.html) | I²C pull-ups (the XIAO has none) |
| R3 | 1 | 10 kΩ | 0805 | [C17414](https://www.lcsc.com/product-detail/C17414.html) | PCF_INT pull-up |
| H1–H4 | 4 | M3 | 3.2 mm NPTH | — | mounting |

All passive LCSC numbers are JLCPCB Basic-library parts (no Extended-part assembly fee), picked for highest stock among matching value/footprint/tolerance. U2 and J1–J16 have no Basic-library equivalent — JST connectors and this specific I/O expander aren't stocked there — so both list the highest-stock Extended part matching the MPN already in the schematic.

Every part above also carries its LCSC number as an `LCSC` field on the schematic symbol itself (not just this table), so it flows straight through KiCad's own BOM export and into `kicad-jlcpcb-tools`. U1 has no LCSC field — the XIAO module isn't an LCSC-stocked part.

Fab target: JLCPCB, 2 layer, 0.2 mm minimum trace and clearance. All passives are
0805 — easier to hand-solder/rework than 0402, at the cost of a slightly larger
board.

## Layout

U1 (XIAO) sits at the bottom of the board, USB-C facing the bottom edge for
external cable access, centered horizontally. U2 (PCF8575) sits directly above
it, also centered. Both are routed and ground-poured; nothing here needs
hand-tweaking before fab.

**Ground plane.** Both copper layers carry a full-board GND pour with a
*solid* pad connection (no thermal-relief spokes) rather than KiCad's default
thermal mode. The PCF8575's 0.65 mm pin pitch is too tight for the standard
2-spoke thermal relief — it kept failing DRC's `starved_thermal` check — and
solid fill also avoids the connectivity islands thermal relief can leave
behind on a densely-routed 2-layer board. A few GND stitching vias tie the two
layers together where no component pad already does.

**Routing.** Autorouted with [Freerouting](https://github.com/freerouting/freerouting)
via its MCP server, driven from a Specctra DSN exported through KiCad's own
`pcbnew.ExportSpecctraDSN`/`ImportSpecctraSES` (the same mechanism as *File →
Export/Import → Specctra*, just scripted). The GND pours export as Specctra
`plane` records, so Freerouting treats GND as already satisfied by the plane
and never draws an explicit GND trace or via for it — the 16 filter caps' GND
legs, and everything else on the net, rely purely on the pour.

**3D models.** J1–J16 and U2 use the standard `.step` models bundled with
their KiCad libraries. U1's OPL library footprint has no dedicated
XIAO ESP32C6 model — only placeholder bodies for other XIAO variants, gated
behind a `${AMZPATH}` environment variable this machine doesn't have
configured — so `3dmodels/seeed-studio-xiao-esp32c6.step` (copied from the
`water-tank-monitor` project's assets) is wired in directly via a
`${KIPRJMOD}`-relative path instead.
