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

**Optional CAN (when enabled, SPI to MCP2515):** 3.3 V; no level shifter needed to MCP2551. See `fis_can.h` for `FIS_CAN_PIN_*`.

| GPIO   | Symbol             | Direction | Function                    |
|--------|--------------------|-----------|-----------------------------|
| GPIO10 | FIS_CAN_PIN_SCK    | Out       | SPI clock to MCP2515        |
| GPIO11 | FIS_CAN_PIN_MOSI   | Out       | SPI MOSI to MCP2515 SI      |
| GPIO12 | FIS_CAN_PIN_MISO   | In        | SPI MISO from MCP2515 SO    |
| GPIO13 | FIS_CAN_PIN_CS     | Out       | SPI chip select (active low) |

MCP2515 SI/SO/SCK/CS connect to the Pico; MCP2515 TXD/RXD connect to MCP2551 TXD/RXD; MCP2551 CANH/CANL to vehicle comfort/infotainment CAN (100 kbit/s).

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

**Feature toggles:** Send `CFG:<name>:0` or `CFG:<name>:1`. Clock/ETA/compass/remain default to 1 (on). CAN defaults to 0 (off).

| Message | Effect |
|---------|--------|
| `CFG:CLOCK:0` / `CFG:CLOCK:1` | Show clock (time from GPS); 0 = do not show clock on FIS |
| `CFG:ETA:0` / `CFG:ETA:1` | ETA in local time on clock line 2 (e.g. ARR14:32) |
| `CFG:COMPASS:0` / `CFG:COMPASS:1` | Compass heading on clock line 2 |
| `CFG:REMAIN:0` / `CFG:REMAIN:1` | Remaining distance on clock line 2 |
| `CFG:CAN:0` / `CFG:CAN:1` | CAN bus support (default off). Set in `firmware/fis_config_defaults.h`; requires external CAN hardware; not on current PCB. |

---

### Optional CAN bus (disabled by default)

CAN support is **off by default**; the default is defined in `firmware/fis_config_defaults.h` (`FIS_CAN_ENABLED_DEFAULT 0`). Enable at runtime with `CFG:CAN:1` only when external CAN hardware is fitted.

**Bit rate:** The B6 comfort/infotainment CAN runs at **100 kbit/s**, not 500 kbit/s. Configure the MCP2515 bit timing for 100 kbit/s.

**Hardware (when implementing):**

- **MCP2515** — CAN controller (SPI to Pico). Set bit timing for 100 kbit/s.
- **MCP2551-I/P (DIP-8)** — CAN transceiver; connects to CAN-H/CAN-L. The MCP2551 runs at 5 V on the bus side but its TXD/RXD logic pins are 3.3 V compatible, so **no level shifter** is needed between the Pico and the MCP2551 (unlike the 3LB side).
- **8 MHz crystal** for the MCP2515, with two **22 pF** capacitors (typical).
- **120 ohm** termination resistor if the Pico board is at a bus endpoint.
- **Decoupling capacitors** on supply pins of MCP2515 and MCP2551.

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
  fis_config.h/.c           – Feature toggles (clock, ETA, compass, remain, CAN); set via CFG:* serial
  fis_config_defaults.h     – Build-time defaults; CAN disabled by default (FIS_CAN_ENABLED_DEFAULT 0)
  fis_can.h/.c              – Optional CAN bus (default off); stub for future MCP2515 + MCP2551 (100 kbit/s)
  fis_bridge.c         – main(), dual-core, decision loop
  tz_table.h/.c        – Timezone bounding boxes and DST rules (flash)
  tz_lookup.h/.c       – Timezone lookup by lat/lon, DST and UTC→local
  local_time.h/.c      – Parse ISO8601, format local time and ETA for FIS
  fis_nav_icons.h      – Pre-generated 1-bit nav icon arrays (64x64 px); see nav-icons/README.md and tools/svg_to_fis_icon.py to add icons
```

---

### Build and flash

**Prerequisites (all platforms)**

- Raspberry Pi Pico SDK installed and `PICO_SDK_PATH` set (or SDK at default location).
- CMake 3.13+ and ARM GCC toolchain (`arm-none-eabi-gcc`). See [Raspberry Pi Pico SDK documentation](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf) for setup.

**Build (all platforms)**

From the project root:

```bash
cd firmware
mkdir -p build && cd build
cmake .. -DPICO_SDK_PATH=/path/to/pico-sdk
make -j4
```

This produces `pico_fis_bridge.uf2` in `firmware/build/`. Use the same steps for the display-test firmware from `firmware-display-test/` (output: `fis_display_test.uf2`).

---

#### Flashing (UF2 via BOOTSEL) — general

1. Power off the Pico 2 W (unplug USB if it is the only power source).
2. Put the board into BOOTSEL (bootloader) mode:
   - Hold the **BOOTSEL** button on the Pico 2 W.
   - While holding it, connect the USB cable (or apply power via VSYS).
   - Release **BOOTSEL** after the board is connected.
3. The board enumerates as a USB mass storage device. Copy the `.uf2` file onto that volume (see platform steps below).
4. The device resets automatically and runs the new firmware. The mass storage volume disappears and the Pico appears as a USB serial (CDC) port at 115200 baud.

---

#### Windows

**Option A: Build in WSL, flash from Windows**

1. Build the firmware inside WSL (e.g. `cd /mnt/c/Users/YourName/FIS-Display/firmware`, then `mkdir build && cd build`, `cmake ..`, `make`). The UF2 is at `firmware/build/pico_fis_bridge.uf2`.
2. In Windows Explorer, open the project folder (e.g. `C:\Users\YourName\FIS-Display\firmware\build\`).
3. Put the Pico 2 W into BOOTSEL (hold BOOTSEL, plug USB, release). A new removable drive appears (e.g. **RPI-RP2** or a drive letter like **D:** or **E:**).
4. Copy the UF2 file onto the drive:
   - Drag `pico_fis_bridge.uf2` from Explorer and drop it onto the **RPI-RP2** (or the assigned drive) window, or
   - In Command Prompt: `copy pico_fis_bridge.uf2 D:\` (replace `D:` with the actual drive letter; use **This PC** in Explorer to see the correct letter).
5. The Pico resets and runs the new firmware. The drive disappears; use a serial terminal (e.g. PuTTY, 115200 baud) on the new COM port to talk to the firmware.

**Option B: Build natively on Windows**

1. Install [MSYS2](https://www.msys2.org/) or use the official Pico Windows toolchain. Install CMake and `arm-none-eabi-gcc` (e.g. via MSYS2: `pacman -S mingw-w64-ucrt-x86_64-arm-none-eabi-gcc cmake make`). Clone the Pico SDK and set `PICO_SDK_PATH`.
2. In a MSYS2 UCRT64 (or similar) shell: `cd firmware`, `mkdir build`, `cd build`, `cmake .. -G "MinGW Makefiles" -DCMAKE_C_COMPILER=arm-none-eabi-gcc`, `make`. The UF2 is in `build\`.
3. Follow steps 3–5 from Option A: BOOTSEL, then copy `pico_fis_bridge.uf2` onto the **RPI-RP2** drive in Explorer (or via `copy` to the correct drive letter).

**Finding the drive letter:** Open **This PC** (or **File Explorer**). After entering BOOTSEL, a new removable disk **RPI-RP2** appears; note its letter (e.g. **E:**). If it does not appear, try another USB cable/port (data-capable); re-enter BOOTSEL.

---

#### macOS

**Prerequisites**

- Xcode Command Line Tools (for `make`, etc.): `xcode-select --install`
- Homebrew: [brew.sh](https://brew.sh)
- ARM toolchain and CMake: `brew install cmake arm-none-eabi-gcc`
- Pico SDK: clone [pico-sdk](https://github.com/raspberrypi/pico-sdk) and set `PICO_SDK_PATH` to the clone path (e.g. in `~/.zshrc`: `export PICO_SDK_PATH=~/pico/pico-sdk`).

**Build**

```bash
cd firmware
mkdir -p build && cd build
cmake .. -DPICO_SDK_PATH=$PICO_SDK_PATH
make -j4
```

The UF2 is at `firmware/build/pico_fis_bridge.uf2`.

**Flash**

1. Put the Pico 2 W into BOOTSEL: hold **BOOTSEL**, connect USB, then release **BOOTSEL**.
2. A volume named **RPI-RP2** appears on the desktop and in Finder under **Locations**.
3. Copy the UF2 onto the volume:
   ```bash
   cp firmware/build/pico_fis_bridge.uf2 /Volumes/RPI-RP2/
   ```
   Or drag `pico_fis_bridge.uf2` from Finder and drop it onto **RPI-RP2** in the Finder sidebar.
4. The Pico resets; the **RPI-RP2** volume disappears. Connect a serial terminal (e.g. `screen /dev/tty.usbmodem* 115200` or Serial.app) to the new CDC port at 115200 baud.

**If the volume does not appear:** Use a known data-capable USB cable; try another port. Run `system_profiler SPUSBDataType` after plugging in (with BOOTSEL held) to confirm the board is seen as "RPI-RP2" or "RP2 Boot".

---

#### Linux

1. Put the Pico 2 W into BOOTSEL (hold BOOTSEL, plug USB, release). The volume is usually mounted at `/media/$USER/RPI-RP2` (Ubuntu/Debian) or similar; check your file manager or `lsblk` / `dmesg`.
2. Copy the UF2:
   ```bash
   cp firmware/build/pico_fis_bridge.uf2 /media/$USER/RPI-RP2/
   ```
   (Adjust the path if your system mounts it elsewhere.)
3. The device resets and runs the new firmware; the mass storage device disappears.

---

**Alternative: picoprobe / OpenOCD**

If you use a Pico as a probe or another debugger, you can flash the `.elf` or `.uf2` with OpenOCD or `picotool` instead of BOOTSEL. The build output in `build/` includes `pico_fis_bridge.elf` and `pico_fis_bridge.uf2`.

---

### Runtime behaviour

**Core 0:** Initialises USB CDC and BTstack SPP (`FIS-Bridge`, PIN `0000`). Polls both for newline-terminated lines, parses `NAV:*`, `BT:*`, and `CFG:*` messages, updates shared `nav_state_t` and `fis_config_t` under a critical section. When CAN is enabled (`CFG:CAN:1`), calls `fis_can_poll` (stub until CAN hardware support is added).

**Core 1:** Runs the 3LB loop: monitors ENA for idle, snapshots `nav_state_t` and `fis_config_t`, then injects (in order of priority) call frame, nav frame, media frame, or clock frame (if clock enabled and GPS time available); otherwise leaves bus to the ECU.

**Injection:** Wait for bus idle (ENA high), drive ENA/CLK/DATA via PIO SM1 for the frame, then release.

---

### Display

The FIS/MFA is a 64x88 pixel monochrome LCD. The firmware sends two 3LB frame types (TLBFISLib-compatible):

- **Radio text (0x81):** Two lines of 8-character text for nav, call, media, and clock.
- **Graphics (0x53 + 0x55):** Claim/clear screen then bitmap blocks. Nav icons in `fis_nav_icons.h` (64x64 px, 1-bit) can be sent with `fis_display_inject_icon(index)` or `fis_display_inject_bitmap(x, y, w, h, data)`.

**Prerequisite:** Code the cluster with VCDS or OBDeleven (Module 17 — Instruments) to enable the navigation source.

**Adding custom icons:** Add an SVG in [nav-icons/](../nav-icons/), run `python3 tools/svg_to_fis_icon.py nav-icons/your_icon.svg`, then add the printed C array and `FIS_ICON_TABLE` entry to `fis_nav_icons.h` and increment `FIS_ICON_COUNT`. See [nav-icons/README.md](../nav-icons/README.md).
