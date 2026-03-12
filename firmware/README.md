## Pico FIS Bridge Firmware (VW Passat B6 FIS/MFA)

> WARNING: This firmware is currently **experimental and untested on a real vehicle**. Use at your own risk and validate behaviour on the bench (cluster on the desk, lab supply) before connecting to a car.

Firmware for Raspberry Pi Pico 2 W (RP2350) that:

- Listens to Navit navigation events over USB CDC serial from a Raspberry Pi running Navit.
- Listens to Android head unit media and call events over **Bluetooth SPP** using the onboard CYW43439 radio via BTstack (no external Bluetooth module required).
- Monitors the Passat B6 3LB bus (ENA/CLK/DATA) to detect idle gaps between ECU frames.
- Injects additional FIS/MFA frames only during idle gaps on the bus (no relays or analog switches required).

This is a **bus co-existence** design: the original ECU retains full control of the FIS/MFA display at all times. The Pico only transmits during free time slots between ECU frames, using the ENA line for bus arbitration as defined by the 3LB protocol.

> **Note:** The OEM radio has been replaced with an Android head unit. The Android head unit does **not** communicate over 3LB. Only the original ECU talks to the FIS/MFA natively over 3LB. The Android head unit communicates with the Pico exclusively via Bluetooth SPP.

---

### Bluetooth pairing

The Pico advertises itself as **`FIS-Bridge`** using classic Bluetooth SPP (Serial Port Profile) via BTstack.

Pairing procedure (one time only):
1. Power up the PCB.
2. On the Android head unit, open Bluetooth settings and scan for devices.
3. Select `FIS-Bridge` from the list.
4. Enter PIN **`0000`** if prompted.
5. Pairing is permanent. On subsequent power-ups the Android head unit reconnects automatically without user interaction.

The Pico auto-accepts all pairing requests — no confirmation is required or possible on the Pico side.

---

### Pinout (Pico 2 W side)

All 3LB lines must be level-shifted between the 5 V car bus and the Pico's 3.3 V GPIO using BS170 N-channel MOSFETs (TO-92, THT) in the standard NXP bidirectional open-drain level shift circuit, with 10 kΩ pull-up resistors to each voltage rail per channel.

**3LB RX — monitor (inputs, sniff ECU frames and detect bus idle):**

- `GPIO0` – `FIS_PIN_ENA` (ENA line, input, monitors bus idle state, via level shifter)
- `GPIO1` – `FIS_PIN_CLK` (CLK line, input to PIO SM0 RX, via level shifter)
- `GPIO2` – `FIS_PIN_DATA` (DATA line, input to PIO SM0 RX, via level shifter)

**3LB TX — inject (outputs, Pico drives FIS/MFA during idle gaps):**

- `GPIO3` – `FIS_PIN_ENA_OUT` (ENA line, output, claims bus before injecting, via level shifter)
- `GPIO4` – `FIS_PIN_CLK_OUT` (CLK line, output, PIO SM1 side-set, via level shifter)
- `GPIO5` – `FIS_PIN_DATA_OUT` (DATA line, output, PIO SM1 out_base, via level shifter)

**USB (Navit serial):**

- Pico native USB CDC — virtual serial port from the Raspberry Pi running Navit (`/dev/ttyACM0`).
- Also used for 5 V power input (VSYS) from the Raspberry Pi USB port.
- No 12 V input is required on the PCB.

**Bluetooth (Android head unit media and call info):**

- Onboard CYW43439 via BTstack SPP.
- No external Bluetooth module. No additional GPIO pins used.

---

### Hardware notes

**Level shifters:** 6× BS170 N-channel MOSFET (TO-92, THT), one per 3LB line (3 RX + 3 TX). Each channel uses two 10 kΩ pull-up resistors — one to 3.3 V, one to 5 V. This implements the standard NXP bidirectional open-drain level shift circuit.

**Inline connectors:** JST PH 2.00 mm pitch, 3-position (single row), one pair each side:
- Cluster side: PHR-3 housing (DigiKey 455-1705-ND) + B3B-PH-K PCB header (DigiKey 455-1126-ND)
- ECU/harness side: same parts
- Crimp terminals: SPH-002T-P0.5S (DigiKey 455-2148TR-ND), fits 24–32 AWG wire
- Any 3-position single-row 2.00 mm pitch THT header is a suitable alternative for the PCB header

**Cluster wiring:** Tap the three 3LB lines at the cluster green connector T32a:
- `T32a pin 30` – DATA
- `T32a pin 31` – CLK
- `T32a pin 32` – ENA

The Pico RX and TX lines for each 3LB signal connect in parallel to the same cluster pins through the level shifters. No relay or analog switch is needed — the 3LB ENA line provides bus arbitration natively.

---

### Source layout

```text
firmware/
  CMakeLists.txt       – Pico SDK build configuration
  fis_3lb_rx.pio       – PIO SM0 program for 3LB RX (CLK/DATA sniffer)
  fis_3lb_tx.pio       – PIO SM1 program for 3LB TX (DATA + side-set CLK)
  fis_rx.h/.c          – RX monitoring and bus idle detection
  fis_display.h/.c     – TX helpers for injecting navigation/media/call frames
  serial_parser.h/.c   – USB CDC line parser, updates nav_state
  nav_state.h          – nav_state_t structure and enums
  fis_bridge.c         – main(), dual-core setup, decision loop
  fis_nav_icons.h      – Pre-generated 1-bit nav icon arrays (64×64 px, PROGMEM)
```

---

### Build instructions

1. Install the Raspberry Pi Pico SDK and toolchain as described in the official documentation.
2. In the `firmware/` directory, ensure `pico_sdk_import.cmake` is available or adjust the include path in `CMakeLists.txt`.
3. Create a build directory and configure CMake:

```bash
mkdir build
cd build
cmake ..
make -j4
```

4. The build produces a `pico_fis_bridge.uf2` file. Flash it to the Pico 2 W by:
   - Holding BOOTSEL while plugging in USB.
   - Mounting the mass-storage device.
   - Copying the `.uf2` file to the device.

---

### Runtime behaviour

**Core 0:**
- Initialises USB CDC and BTstack SPP (`FIS-Bridge`, PIN `0000`).
- Reads text lines from USB CDC (Navit) and Bluetooth SPP (Android head unit) non-blocking.
- Parses messages:
  - `NAV:TURN:<code>` — upcoming maneuver type
  - `NAV:DIST:<metres>` — distance to next turn
  - `NAV:STREET:<name>` — next street name (max 20 chars)
  - `NAV:STATUS:<status>` — Navit routing status
  - `NAV:ETA:<unix_ts>` — estimated arrival time
  - `NAV:REMAIN:<metres>` — total remaining distance
  - `BT:TRACK:<info>` — media track info from Android head unit
  - `BT:CALL:<caller>` — incoming call caller ID from Android head unit
  - `BT:CALLEND` — call ended
- Updates shared `nav_state_t` protected by a mutex.

**Core 1:**
- Runs the 3LB bus loop continuously.
- Uses `fis_rx_task()` and `fis_bus_idle()` to monitor ENA and detect inter-frame gaps between ECU transmissions.
- When bus is idle, snapshots `nav_state_t` and decides what to inject:
  - Active call → `fis_display_inject_call()` (highest priority)
  - Active navigation (`routing` or `recalculating`) → `fis_display_inject_nav()` with icon from `fis_nav_icons.h`
  - No nav active, media info present → `fis_display_inject_media()`
  - Otherwise → do nothing, ECU frames reach FIS/MFA unmodified

**Injection procedure:** Wait for ENA LOW (bus idle) → raise ENA → transmit frame via PIO SM1 → lower ENA → ECU resumes normally on next frame.

---

### Display

The FIS display (also referred to as MFA — Multifunktionsanzeige) is a 64×88 pixel monochrome LCD. Navigation icons are stored as 1-bit 64×64 px arrays in `fis_nav_icons.h` and rendered using `GraphicFromArray(x, y, width, height, array, 0)` where the last parameter `0` indicates flash/PROGMEM storage.

> **Prerequisite:** The cluster must be coded with VCDS or OBDeleven (Module 17 — Instruments) to enable the navigation source. Without this coding the navigation menu slot in the FIS/MFA is not activated.
