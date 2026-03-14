## Pico FIS Bridge Firmware (VW Passat B6 FIS/MFA)

> WARNING: This firmware is currently **experimental and untested on a real vehicle**. Use at your own risk and validate behaviour on the bench (cluster on the desk, lab supply) before connecting to a car.

PCB files and schematic diagram will be made soon.

Firmware for **Raspberry Pi Pico 2 W** (RP2350) that:

- Listens to Navit navigation events over USB CDC serial from a host (Raspberry Pi, Linux carputer, or Android with Navit).
- Listens to Android head unit media and call events over **Bluetooth SPP** using the onboard CYW43439 via BTstack (no external Bluetooth module).
- Receives the same **serial protocol** (NAV:*, BT:*) on both USB CDC and Bluetooth SPP; fully integrated on both transports.
- Monitors the Passat B6 3LB bus (ENA/CLK/DATA) to detect idle gaps between ECU frames.
- Injects FIS/MFA frames only during idle gaps (no relays or analog switches).

The Pico is a **pure middleman**: the host runs Navit with D-Bus and a bridge script that forwards the protocol to the Pico over USB or Bluetooth.

> **Note:** The OEM radio has been replaced with an Android head unit. The Android head unit does **not** communicate over 3LB. Only the original ECU talks to the FIS/MFA natively over 3LB. The Android head unit communicates with the Pico via Bluetooth SPP.

---

### Bluetooth pairing

The Pico advertises as **`FIS-Bridge`** using classic Bluetooth SPP (BTstack, onboard CYW43439).

Pairing (one time only):
1. Power up the PCB.
2. On the Android head unit, open Bluetooth settings and scan for devices.
3. Select `FIS-Bridge`.
4. Enter PIN **`0000`** if prompted.
5. Pairing is permanent; the head unit reconnects automatically on later power-ups.

The Pico auto-accepts all pairing requests — no user interaction on the Pico side.

---

### Pinout (Pico 2 W)

All 3LB lines must be level-shifted between the 5 V car bus and the Pico's 3.3 V GPIO using BS170 N-channel MOSFETs (TO-92, THT) in the standard NXP bidirectional open-drain level shift circuit, with 2.2 kOhm pull-ups to each rail per channel. Use metal film or SMD thick/thin film resistors for low parasitic inductance; avoid wirewound types.

**3LB RX (inputs):**

- `GPIO0` – `FIS_PIN_ENA` (ENA, input, bus idle, via level shifter)
- `GPIO1` – `FIS_PIN_CLK` (CLK, input to PIO SM0, via level shifter)
- `GPIO2` – `FIS_PIN_DATA` (DATA, input to PIO SM0, via level shifter)

**3LB TX (outputs):**

- `GPIO3` – `FIS_PIN_ENA_OUT` (ENA, output, claim bus before inject, via level shifter)
- `GPIO4` – `FIS_PIN_CLK_OUT` (CLK, PIO SM1 side-set, via level shifter)
- `GPIO5` – `FIS_PIN_DATA_OUT` (DATA, PIO SM1 out_base, via level shifter)

**USB:** Pico USB CDC for Navit/host protocol; also 5 V power (VSYS). No 12 V on the PCB.

**Bluetooth:** Onboard CYW43439 via BTstack SPP. Same NAV/BT protocol as USB; no extra GPIO.

---

### Transport (host to Pico)

- **USB CDC** — Host uses `/dev/ttyACM0` (Linux) or equivalent; 115200 baud, newline-terminated.
- **Bluetooth SPP** — Fully integrated. Pico advertises as `FIS-Bridge`, PIN `0000`; host pairs once and sends the same NAV/BT protocol over SPP. BTstack runs alongside USB CDC.

---

### Serial protocol (115200 baud, newline-terminated)

Same on USB CDC and Bluetooth SPP:

| Message | Meaning |
|---------|---------|
| `NAV:TURN:<code>` | Maneuver type |
| `NAV:DIST:<metres>` | Distance to next turn |
| `NAV:STREET:<name>` | Next street name (max 20 chars) |
| `NAV:STATUS:<status>` | `routing`, `recalculating`, `no_route`, etc. |
| `NAV:ETA:<unix_ts>` | Estimated arrival (Unix ts); converted to local time, shown as e.g. ARR14:32 on clock line 2 |
| `NAV:REMAIN:<metres>` | Remaining distance to destination (Navit `destination_length`); shown on clock line 2 |
| `NAV:HEAD:<degrees>` | Heading 0-360 (Navit `position_direction`); shown as compass (N, NE, E, ...) on clock line 2 |
| `NAV:TIME:<iso8601>` | GPS time UTC (e.g. `2026-03-14T12:34:56Z`), from Navit `position_time_iso8601` |
| `NAV:POS:<lat>,<lon>` | GPS position decimal degrees, from Navit `position_coord_geo` (for timezone lookup) |
| `BT:TRACK:<info>` | Media track info |
| `BT:CALL:<caller>` | Incoming call caller ID |
| `BT:CALLEND` | Call ended |

When `NAV:TIME` and optionally `NAV:POS` are received, the Pico looks up the timezone from a compact bounding-box table stored in flash, computes DST transitions algorithmically (EU rule: last Sunday March/October at 01:00 UTC), converts UTC to local time, and can inject a clock display on the FIS when no nav/media/call is being shown. ETA (Navit `eta`, Unix timestamp) is converted to local time and shown on clock line 2 (e.g. ARR14:32). No network, no OS timezone database, no user configuration.

**Feature toggles:** Send `CFG:<name>:0` or `CFG:<name>:1`. All default to 1 (on).

| Message | Effect |
|---------|--------|
| `CFG:CLOCK:0` / `CFG:CLOCK:1` | Show clock (time from GPS); 0 = do not show clock on FIS |
| `CFG:ETA:0` / `CFG:ETA:1` | ETA in local time on clock line 2 (e.g. ARR14:32) |
| `CFG:COMPASS:0` / `CFG:COMPASS:1` | Compass heading on clock line 2 |
| `CFG:REMAIN:0` / `CFG:REMAIN:1` | Remaining distance on clock line 2 |

---

### Hardware notes

**Level shifters:** 6x BS170 (TO-92, THT), one per 3LB line (3 RX + 3 TX). Two 2.2 kOhm pull-ups per channel (3.3 V and 5 V), preferably metal film or SMD thick/thin film (avoid wirewound). See `BOM.md`.

**Inline connectors:** JST PH 2.00 mm, 3-position; one pair each side (cluster and harness). PHR-3 housing, B3B-PH-K header, SPH-002T-P0.5S crimps (24–32 AWG).

**Cluster:** Tap 3LB at green T32a — pin 30 DATA, 31 CLK, 32 ENA. Pico RX/TX connect in parallel through level shifters; no relay or analog switch.

---

### Source layout

```text
firmware/
  CMakeLists.txt       – Pico SDK build (Pico 2 W)
  fis_3lb_rx.pio       – PIO SM0: 3LB RX (sniffer)
  fis_3lb_tx.pio       – PIO SM1: 3LB TX (inject)
  fis_rx.h/.c          – Bus idle detection (ENA)
  fis_display.h/.c     – Frame build and inject (nav/media/call/clock)
  serial_parser.h/.c   – USB CDC and Bluetooth SPP line parser (NAV/BT/CFG)
  nav_state.h          – nav_state_t, enums
  fis_config.h/.c      – Feature toggles (clock, ETA, compass, remain); set via CFG:* serial
  fis_bridge.c         – main(), dual-core, decision loop
  tz_table.h/.c        – Timezone bounding boxes and DST rules (flash)
  tz_lookup.h/.c       – Timezone lookup by lat/lon, DST and UTC→local
  local_time.h/.c      – Parse ISO8601, format local time and ETA for FIS
  fis_nav_icons.h      – Pre-generated 1-bit nav icon arrays (64x64 px)
```

---

### Build and flash

**Prerequisites**

- Raspberry Pi Pico SDK installed and `PICO_SDK_PATH` set (or SDK at default location, e.g. `~/pico/pico-sdk`).
- CMake 3.13+, ARM GCC toolchain (e.g. `arm-none-eabi-gcc`). The Pico SDK install guide covers toolchain setup.

**Build**

From the project root:

```bash
cd firmware
mkdir -p build && cd build
cmake ..
make -j4
```

This produces `pico_fis_bridge.uf2` in `firmware/build/`.

**Flash (UF2 via BOOTSEL)**

1. Power off the Pico 2 W (unplug USB if it is the only power source).
2. Put the board into BOOTSEL (bootloader) mode:
   - Hold the **BOOTSEL** button on the Pico 2 W.
   - While holding it, connect the USB cable (or apply power via VSYS).
   - Release **BOOTSEL** after the board is connected.
3. The board should enumerate as a USB mass storage device (e.g. `RPI-RP2` on Linux/macOS, or a new drive letter on Windows).
4. Copy the UF2 file onto that volume:
   - **Linux/macOS:** `cp pico_fis_bridge.uf2 /media/$USER/RPI-RP2/` (path may vary; check `dmesg` or your file manager).
   - **Windows:** Drag `pico_fis_bridge.uf2` onto the `RPI-RP2` drive in Explorer.
5. The device will automatically reset and run the new firmware. The mass storage device will disappear and the Pico will appear as a USB serial (CDC) port at 115200 baud.

**Alternative: picoprobe / OpenOCD**

If you use a Pico as a probe or another debugger, you can flash the `.elf` or `.uf2` with OpenOCD or `picotool` instead of BOOTSEL. The build output in `build/` includes `pico_fis_bridge.elf` and `pico_fis_bridge.uf2`.

---

### Runtime behaviour

**Core 0:** Initialises USB CDC and BTstack SPP (`FIS-Bridge`, PIN `0000`). Polls both for newline-terminated lines, parses `NAV:*`, `BT:*`, and `CFG:*` messages, updates shared `nav_state_t` and `fis_config_t` under a critical section.

**Core 1:** Runs the 3LB loop: monitors ENA for idle, snapshots `nav_state_t` and `fis_config_t`, then injects (in order of priority) call frame, nav frame, media frame, or clock frame (if clock enabled and GPS time available); otherwise leaves bus to the ECU.

**Injection:** Wait for bus idle (ENA high), drive ENA/CLK/DATA via PIO SM1 for the frame, then release.

---

### Display

The FIS/MFA is a 64x88 pixel monochrome LCD. Nav icons are 1-bit 64x64 arrays in `fis_nav_icons.h`, rendered with `GraphicFromArray(x, y, width, height, array, 0)` (0 = flash/PROGMEM).

**Prerequisite:** Code the cluster with VCDS or OBDeleven (Module 17 — Instruments) to enable the navigation source.
