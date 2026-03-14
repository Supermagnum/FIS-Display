# FIS Display Test Firmware

Standalone firmware that **cycles through all display modes** supported by the main FIS bridge every **4 seconds**. Use it to verify that the cluster display works and that every screen type (navigation text, caller ID, audio track, clock) is shown correctly.

No serial input or host is required. Demo content is generated on the Pico.

---

## What it cycles through

Every 4 seconds the display advances to the next phase:

- **Phases 0-4**: Nav text (distance + maneuver + street).
- **Phases 5-37**: All 33 nav/status icons from `fis_nav_icons.h` (bitmap graphics, 64x64 px each).
- **Phase 38**: Call (CALL / Test Caller).
- **Phase 39**: Media (TRACK / Test Track).
- **Phase 40**: Clock (local time + line 2).

After phase 40 it wraps to 0. Full cycle is 41 x 4 = 164 seconds.

---

## Building

Same pinout and hardware as the main firmware (see [firmware/README.md](../firmware/README.md)). Build from this directory using the Pico SDK:

```bash
mkdir build
cd build
cmake .. -DPICO_SDK_PATH=/path/to/pico-sdk
make
```

Output: `build/fis_display_test.uf2`.

## Flashing

Use the same BOOTSEL procedure as the main firmware; only the UF2 file name differs.

- **Windows:** Put Pico 2 W into BOOTSEL (hold BOOTSEL, plug USB, release). Copy `fis_display_test.uf2` onto the **RPI-RP2** drive in Explorer (or `copy fis_display_test.uf2 D:\` with the correct drive letter). Full steps: [firmware/README.md - Windows](../firmware/README.md#windows).
- **macOS:** Put Pico 2 W into BOOTSEL, then `cp fis_display_test.uf2 /Volumes/RPI-RP2/` or drag the file onto the **RPI-RP2** volume in Finder. Prerequisites and details: [firmware/README.md - macOS](../firmware/README.md#macos).
- **Linux:** `cp fis_display_test.uf2 /media/$USER/RPI-RP2/` (or your mount path) after entering BOOTSEL.

---

## Use case

- **Bench test**: Connect the PCB to a Passat B6 cluster (with correct level shifters and 3LB wiring). Power up; the display should cycle through all modes so you can check segments and contrast.
- **No host**: No USB or Bluetooth host is needed; the Pico runs alone.
- **Same 3LB behaviour**: Uses the same 3LB RX (idle detection) and TX (inject) logic as the main firmware, so you also verify bus timing and coexistence.

The main firmware sends the same frame types (radio text 0x81); this test only replaces the content source with fixed demo data and a 4 s timer.
