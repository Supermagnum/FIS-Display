## Pico FIS Bridge Firmware (VW Passat B6 FIS)

> WARNING: This firmware is currently **experimental and untested on a real vehicle**. Use at your own risk and validate behaviour on the bench (cluster on the desk, lab supply) before connecting to a car.

Firmware for Raspberry Pi Pico 2 W (RP2350) that:

- Listens to Navit navigation and Android media/call events over USB CDC and Bluetooth (HC‑05 on UART1).
- Monitors the Passat B6 3LB bus (ENA/CLK/DATA) in parallel with the OEM radio.
- Injects additional FIS frames only during idle gaps on the bus (no relays or analog switches).

This is a **true bus co‑existence** design: the OEM radio keeps full control of the FIS, and the Pico only talks during free time slots.

---

### Pinout (Pico 2 W side)

All 3LB lines must be level‑shifted between the 5 V car bus and the Pico’s 3.3 V GPIO.

**3LB monitor and TX:**

- `GPIO0` – `FIS_PIN_ENA` (ENA from cluster/radio, input only, via level shifter)
- `GPIO1` – `FIS_PIN_CLK` (CLK from cluster/radio, input to PIO RX, via level shifter)
- `GPIO2` – `FIS_PIN_DATA` (DATA from cluster/radio, input to PIO RX, via level shifter)
- `GPIO3` – `FIS_PIN_ENA_OUT` (ENA to cluster, output via level shifter)
- `GPIO4` – `FIS_PIN_CLK_OUT` (CLK to cluster, output via level shifter, PIO side‑set)
- `GPIO5` – `FIS_PIN_DATA_OUT` (DATA to cluster, output via level shifter, PIO out_base)

**Bluetooth (Android media):**

- `GPIO8` – `UART1 RX` (from HC‑05 TXD)
- `GPIO9` – `UART1 TX` (to HC‑05 RXD, through 1 kΩ + 2 kΩ divider)

**USB (Navit):**

- Pico’s USB CDC is used as a virtual serial port from the Raspberry Pi running Navit.

---

### Cluster and radio wiring (high level)

- Tap the three 3LB lines at the cluster green connector T32a:
  - `T32a/32` – ENA
  - `T32a/31` – CLK
  - `T32a/30` – DATA
- Leave the OEM radio wired as stock.
- Add level shifters between these three cluster pins and the Pico pins listed above.
- The Pico’s ENA/CLK/DATA outputs are connected **in parallel** to the same cluster pins, through the level shifters.
- No relays or analog switches are required; the firmware only transmits when the bus is idle.

---

### Source layout

```text
firmware/
  CMakeLists.txt       – Pico SDK build configuration
  fis_3lb_rx.pio       – PIO SM0 program for 3LB RX (CLK/DATA sniffer)
  fis_3lb_tx.pio       – PIO SM1 program for 3LB TX (DATA + side‑set CLK)
  fis_rx.h/.c          – RX monitoring and bus idle detection
  fis_display.h/.c     – TX helpers for injecting navigation/media/call frames
  serial_parser.h/.c   – USB CDC + UART1 line parser, updates nav_state
  nav_state.h          – nav_state_t structure and enums
  fis_bridge.c         – main(), dual‑core setup, decision loop
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
   - Mounting the mass‑storage device.
   - Copying the `.uf2` file to the device.

---

### Runtime behaviour

- **Core 0**:
  - Initialises USB CDC and UART1.
  - Reads text lines from both interfaces (non‑blocking).
  - Parses messages like:
    - `NAV:TURN:<code>`
    - `NAV:DIST:<metres>`
    - `NAV:STREET:<name>`
    - `NAV:STATUS:<status>`
    - `NAV:ETA:<unix_ts>`
    - `NAV:REMAIN:<metres>`
    - `BT:TRACK:<info>`
    - `BT:CALL:<caller>`
    - `BT:CALLEND`
  - Updates a shared `nav_state_t` protected by a critical section.

- **Core 1**:
  - Runs the 3LB loop.
  - Uses `fis_rx_task()` and `fis_bus_idle()` to monitor ENA and detect inter‑frame gaps.
  - When the bus is idle, snapshots `nav_state_t` and decides what to inject:
    - Active call → `fis_display_inject_call()`.
    - Active navigation (`routing`/`recalculating`) → `fis_display_inject_nav()`.
    - No nav, but media present → `fis_display_inject_media()`.

The current `fis_display.c` implementation sends simple, length‑prefixed text frames over the 3LB transport. These are designed to be replaced with proper BAP/3LB framing and icon graphics using the official VW Navigation_SD function catalogue once the exact opcodes and layouts are chosen.

