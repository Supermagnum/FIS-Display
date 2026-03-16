# FIS-Display
VW Passat B6 FIS/MFA Display — Hardware Connections and Software Protocols.

## Disclaimer

> The creator takes no responsibility for damages, immobile car, injury, or any other loss arising from the use of this project, its firmware, PCB design, or documentation. Use at your own risk.

## What this project does

[Navit](https://github.com/navit-gps/navit) is an open-source, modular turn-by-turn navigation engine that can run on Linux, Android, and other platforms. This project uses Navit on a host device (with D-Bus enabled) to drive the car's FIS/MFA display.

This repository provides firmware, PCB design, and documentation to show Navit navigation (and optionally media, call, clock) on the VW Passat B6 (3C) FIS/MFA display. 

A host device runs Navit with D-Bus and sends a simple serial protocol to a Raspberry Pi Pico 2 W over USB or Bluetooth (pair once as FIS-Bridge). 
The Pico injects frames onto the car's 3LB bus during idle gaps so the cluster shows turn-by-turn directions, street name, distance, and maneuver icons; when not navigating, it can show media track info, incoming call caller ID, or a clock screen with GPS time, ETA (e.g. ARR 14:32), remaining distance, and compass heading. 

The firmware supports CAN bus (MCP2561 on GPIO 11/12, 100 kbit/s, software CAN 2.0A send/receive); it is disabled by default and can be enabled with CFG:CAN:1. The Pico is a middleman only; all navigation logic stays on the host. When CAN bus is enabled, it can adjust your clock automatically. 

A separate display-test firmware is provided for bench-testing the cluster without a Navit host.

PCB and schematic are designed in [KiCad](https://www.kicad.org/). Design files and Gerbers are in [pcb-files/](pcb-files/). See [Gerber files and PCB manufacturing](#gerber-files-and-pcb-manufacturing) for what they are and how to use them.

> **Note:** The firmware in `firmware/` is **experimental and untested on a real vehicle**. Validate on the bench before connecting to the car. See [firmware/README.md](firmware/README.md) for build and flash instructions; that file also explains how to install the Raspberry Pi Pico SDK if it is missing.

## Gerber files and PCB manufacturing

**What are Gerber files?** Gerber is the standard format used by PCB manufacturers to produce printed circuit boards. Each file describes one layer of the board (copper, solder mask, silkscreen, etc.) as vector graphics. The manufacturer combines these files to fabricate and assemble the PCB. This repository provides Gerbers exported from the KiCad project in [pcb-files/FIS-display/](pcb-files/FIS-display/).

**What is in this repo:** In [pcb-files/FIS-display/gerbers/](pcb-files/FIS-display/gerbers/) you will find:

- **Copper layers:** `F_Cu.gbr` (top copper), `B_Cu.gbr` (bottom copper)
- **Solder mask:** `F_Mask.gbr`, `B_Mask.gbr`
- **Silkscreen:** `F_Silkscreen.gbr`, `B_Silkscreen.gbr`
- **Solder paste (SMD stencil):** `F_Paste.gbr`, `B_Paste.gbr`
- **Board outline:** `Edge_Cuts.gbr`
- **Drill files:** `*.drl` (through-hole and non-plated drill data)

**How to use them:**

1. **Ordering PCBs:** Zip the contents of the `gerbers/` folder (all `.gbr` and `.drl` files, and the `.gbrjob` file if the manufacturer supports it) and upload the zip to a PCB fab (e.g. JLCPCB, PCBWay, OSH Park, or similar). Select the correct units (usually mm) and board thickness; the fab will use the Gerbers to produce the boards. Use the same BOM as in [firmware/BOM.md](firmware/BOM.md) for sourcing components.

2. **Viewing without KiCad:** You can inspect the layers with a Gerber viewer (e.g. [GerberView](https://www.gerber-viewer.com/), or the online viewer many fabs provide) to check traces, pads, and outline before ordering.

3. **Editing the design:** Open the KiCad project in [pcb-files/FIS-display/](pcb-files/FIS-display/) (`.kicad_pro`, `.kicad_sch`, `.kicad_pcb`) in [KiCad](https://www.kicad.org/). After any change, re-export Gerbers from KiCad (File → Plot, then generate drill files) and replace the files in `gerbers/` before ordering new boards.

---

## Table of contents

- [Disclaimer](#disclaimer)
- [What this project does](#what-this-project-does)
- [Gerber files and PCB manufacturing](#gerber-files-and-pcb-manufacturing)
- [Project structure](#project-structure)
- [1. System Overview](#1-system-overview)
- [2. Host Device — Navit with D-Bus](#2-host-device--navit-with-d-bus)
  - [2.1 Supported Platforms](#21-supported-platforms)
  - [2.2 D-Bus Service Details](#22-d-bus-service-details)
  - [2.3 Key Attributes to Subscribe To](#23-key-attributes-to-subscribe-to)
  - [2.4 Navigation Status Values](#24-navigation-status-values)
  - [2.5 D-Bus Listener to Pico Bridge Script](#25-d-bus-listener--pico-bridge-script)
- [3. Serial Protocol (Host to Pico, 115200 baud)](#3-serial-protocol-host--pico-115200-baud-newline-terminated-ascii)
  - [3.1 Eco mode icon (driver-break, D-Bus)](#31-eco-mode-icon-driver-break-d-bus)
- [4. Transport — USB CDC or Bluetooth SPP](#4-transport--usb-cdc-or-bluetooth-spp)
  - [USB CDC](#usb-cdc)
  - [Bluetooth SPP](#bluetooth-spp)
- [5. Hardware](#5-hardware)
  - [5.1 Controller — Raspberry Pi Pico 2 W](#51-controller--raspberry-pi-pico-2-w)
  - [5.2 The 3LB (Three-Line Bus)](#52-the-3lb-three-line-bus)
  - [5.3 Level Shifting](#53-level-shifting)
  - [5.4 Instrument Cluster Connector (Green T32a)](#54-instrument-cluster-connector-green-t32a)
  - [5.5 3LB connection: pigtails or wire-to-wire (Molex kit)](#55-3lb-connection-pigtails-or-wire-to-wire-molex-kit)
  - [5.6 Pico 2 W GPIO Pinout](#56-pico-2-w-gpio-pinout)
  - [5.7 Cluster Coding Prerequisite](#57-cluster-coding-prerequisite)
- [6. The 3LB Protocol](#6-the-3lb-protocol)
  - [6.1 Open Source Libraries (Reference)](#61-open-source-libraries-reference)
  - [6.2 Display](#62-display)
  - [6.3 OEM radio: CAN time and display (reference only)](#63-oem-radio-can-time-and-display-reference-only)
  - [6.4 Other potentially useful CAN messages (reference only)](#64-other-potentially-useful-can-messages-reference-only)
- [7. Firmware Runtime Behaviour](#7-firmware-runtime-behaviour)
- [8. Software Stack Summary](#8-software-stack-summary)
- [9. Key References](#9-key-references)

---

## Project structure

This repository uses **no symlinks**; all paths are normal directories and files so that every system can clone and use it without symlink support.

| Item | Description |
|------|-------------|
| [firmware/](firmware/) | Main Pico 2 W firmware: 3LB RX/TX, serial protocol, nav/media/clock injection, graphics. |
| [firmware/README.md](firmware/README.md) | Build, flash, pinout, and firmware behaviour. |
| [firmware/BOM.md](firmware/BOM.md) | Bill of materials for the PCB. |
| [firmware-display-test/](firmware-display-test/) | Standalone display test firmware: cycles through all modes (nav text, icons, call, media, clock) every 4 seconds. |
| [firmware-display-test/README.md](firmware-display-test/README.md) | How to build and use the display test. |
| [nav-icons/](nav-icons/) | Navigation and status icon SVGs (sources for the bitmaps in `firmware/fis_nav_icons.h`). See [nav-icons/README.md](nav-icons/README.md) for adding new icons. |
| [tools/](tools/) | Build/convert helpers. [tools/svg_to_fis_icon.py](tools/svg_to_fis_icon.py) converts SVG to the 64x64 1-bit C array format. [tools/navit_dbus_to_pico_bridge.py](tools/navit_dbus_to_pico_bridge.py) runs on the host and forwards Navit D-Bus (including eco_mode_fuel_enabled) to the Pico over serial. |
| [PQ35_46_ACAN_KMatrix_V5.20.6F_20160530_MH.xlsx](PQ35_46_ACAN_KMatrix_V5.20.6F_20160530_MH.xlsx) | VW/Audi PQ35/46 CAN matrix (reference). |
| [PQ35_46_ACAN_Glossary_DE_EN.md](PQ35_46_ACAN_Glossary_DE_EN.md) | German–English translation table for the CAN matrix document. |
| [pcb-files/](pcb-files/) | PCB design (KiCad) and [Gerber files](pcb-files/FIS-display/gerbers/) for manufacturing. See [Gerber files and PCB manufacturing](#gerber-files-and-pcb-manufacturing). |

---

## 1. System Overview

```
┌──────────────────────────────────────────────────────────┐
│  Host device running Navit with D-Bus enabled            │
│                                                          │
│  Examples:                                               │
│  • Android head unit (Navit built with D-Bus support)    │
│  • Linux carputer / Raspberry Pi running Navit           │
│  • Any platform where libbinding_dbus.so is active       │
│                                                          │
│  D-Bus listener translates Navit signals to serial       │
│  protocol and forwards to Pico over USB or Bluetooth     │
└────────────┬─────────────────────────┬───────────────────┘
             │                         │
     USB CDC serial            Bluetooth SPP
     /dev/ttyACM0              (onboard CYW43439)
     (also powers Pico)        "FIS-Bridge" PIN 0000
             │                         │
             └──────────┬──────────────┘
                        │
               ┌────────▼─────────┐
               │ Raspberry Pi     │
               │ Pico 2 W         │
               │ (RP2350)         │
               │                  │
               │ Middleman:       │
               │ serial -> 3LB;   │
               │ optional: GPIO   │
               │ 11/12 -> MCP2561 │
               └────────┬─────────┘
                        │
         ┌──────────────┼──────────────┐
         │              │              │
         ▼              │              ▼
 3LB (ENA/CLK/DATA)     │      Optional CAN (if enabled):
 via BS170 level        │      GPIO 11 (TX), 12 (RX) ->
 shifters (3.3V<->5V)   │      MCP2561 (TXD/RXD) -> CAN-H/CAN-L
         │              │              │
         ▼              │              ▼
 ┌──────────────────┐   │   ┌──────────────────────────┐
 │ VW Passat B6 (3C) │   │   │ Komfort-CAN (K-CAN)      │
 │ FIS/MFA cluster   │◄──┘   │ 100 kbit/s (when fitted) │
 │ 64x88 px, 1-bit   │       └──────────────────────────┘
 └────────┬──────────┘
          │
          ▼
 ┌───────────────────────┐
 │ Original ECU          │
 │ (talks 3LB natively,  │
 │  Pico co-exists via   │
 │  ENA arbitration)     │
 └───────────────────────┘
```

The Pico 2 W is a **pure middleman**. It has no navigation intelligence — it only receives
the serial protocol from the host device and injects the translated frames onto the 3LB bus.

All navigation logic stays on the host device running Navit. Optionally, when CAN is enabled
(see [5.6 Pico 2 W GPIO Pinout](#56-pico-2-w-gpio-pinout)), the Pico can communicate with the
vehicle Komfort-CAN (Comfort CAN / K-CAN, 100 kbit/s) via GPIO 11/12 to the MCP2561 (TXD/RXD only).

The original ECU continues to talk to the FIS/MFA natively over 3LB at all times. The Pico co-exists on the bus using the ENA line for arbitration — no relay or analog switch is needed.

> **Note:** The OEM radio has been replaced with an aftermarket head unit. The head unit does
> **not** communicate over 3LB. Only the original ECU talks to the FIS/MFA natively over 3LB.

---

## 2. Host Device — Navit with D-Bus

### 2.1 Supported Platforms

Navit can be built with D-Bus support (`libbinding_dbus.so`) on multiple platforms:

| Platform | Notes |
|----------|-------|
| Android head unit | Navit built with D-Bus support. Android includes D-Bus (`android_external_dbus`) |
| Linux carputer | Native D-Bus session bus, full support |
| Raspberry Pi (Linux) | Native D-Bus session bus, full support |
| Any Linux-based system | Native D-Bus session bus, full support |

Enable D-Bus in `navit.xml`:
```xml
<plugin path="$NAVIT_LIBDIR/*/${NAVIT_LIBPREFIX}libbinding_dbus.so" active="yes"/>
```

### 2.2 D-Bus Service Details

| Item | Value |
|------|-------|
| Service name | `org.navit_project.navit` |
| Bus type | Session bus |
| Main object | `/org/navit_project/navit/default_navit` |
| Route object | `/org/navit_project/navit/default_navit/default_route` |
| Vehicle object | `/org/navit_project/navit/default_navit/default_vehicle` |

### 2.3 Key Attributes to Subscribe To

| Attribute | Meaning |
|-----------|---------|
| `navigation_next_turn` | Upcoming maneuver type |
| `navigation_distance_turn` | Metres to next turn |
| `navigation_street_name` | Next street name |
| `navigation_street_name_systematic` | Route number |
| `navigation_status` | Routing state |
| `position_speed` | Current speed |
| `eta` | Estimated arrival (Unix timestamp); sent as `NAV:ETA`, shown in local time on FIS clock line |
| `destination_length` | Remaining distance to destination (m); sent as `NAV:REMAIN`, shown on FIS clock line |
| `position_direction` | Heading in degrees (0-360); sent as `NAV:HEAD`, shown as compass (N, NE, E, ...) on FIS |
| `position_time_iso8601` | GPS time UTC, format `YYYY-MM-DDTHH:MM:SSZ` (for FIS clock) |
| `position_coord_geo` | GPS latitude/longitude (for timezone lookup; vehicle attribute) |

### 2.4 Navigation Status Values

| Value | Meaning |
|-------|---------|
| `no_route` | No active route |
| `no_destination` | No destination set |
| `position_wait` | Waiting for GPS |
| `calculating` | Initial calculation |
| `recalculating` | Rerouting |
| `routing` | Active guidance |

### 2.5 D-Bus Listener to Pico Bridge Script

A bridge script runs on the host device and forwards Navit D-Bus attributes to the Pico over USB CDC or Bluetooth SPP. This repo includes **`tools/navit_dbus_to_pico_bridge.py`**, which implements the protocol and **eco_mode_fuel_enabled** support (Driver Break plugin): when `get_attr("eco_mode_fuel_enabled")` is true and a route has just been calculated (`navigation_status` = `routing`), it sends the eco icon for a short period (default 2.5 s), then the real first turn and distance so the FIS never shows eco instead of routing.

**Run (host):**
```bash
python3 tools/navit_dbus_to_pico_bridge.py --port /dev/ttyACM0   # USB CDC
python3 tools/navit_dbus_to_pico_bridge.py --port /dev/rfcomm0 --eco-seconds 2.5   # Bluetooth SPP
```

**Requirements:** Python 3, `pyserial`, PyGObject (`gi.repository.GLib`), `dbus`. Optional: Driver Break plugin in Navit for `eco_mode_fuel_enabled` (OBD-II / J1939 / MegaSquirt or adaptive fuel learning).

**Inline example (minimal, no eco logic)** for reference:

```python
#!/usr/bin/env python3
"""
Navit D-Bus listener -> Pico 2 W serial bridge.
Runs on any platform where Navit is built with libbinding_dbus.so active:
Android (with D-Bus support), Linux carputer, Raspberry Pi, etc.
Transport: USB CDC or Bluetooth SPP -- configure SERIAL_PORT below.
"""

import serial
import dbus
import dbus.mainloop.glib
from gi.repository import GLib

SERIAL_PORT = "/dev/ttyACM0"   # USB CDC, or e.g. /dev/rfcomm0 for Bluetooth SPP
BAUD = 115200

WATCHED = [
    "navigation_next_turn",
    "navigation_distance_turn",
    "navigation_street_name",
    "navigation_status",
    "eta",
    "destination_length",      # remaining distance (m) -> NAV:REMAIN, shown on FIS clock line
    "position_direction",      # heading 0-360 -> NAV:HEAD, shown as compass (N, NE, E, ...)
    "position_time_iso8601",   # UTC GPS time for FIS clock (format YYYY-MM-DDTHH:MM:SSZ)
    "position_coord_geo",      # lat/lon for timezone lookup (vehicle attribute)
]

MANEUVER_MAP = {
    "straight":          "straight",
    "turn_left":         "turn_left",
    "turn_right":        "turn_right",
    "turn_slight_left":  "slight_left",
    "turn_slight_right": "slight_right",
    "turn_sharp_left":   "sharp_left",
    "turn_sharp_right":  "sharp_right",
    "u_turn":            "u_turn",
    "keep_left":         "keep_left",
    "keep_right":        "keep_right",
    "destination":       "destination",
}

class NavitBridge:
    def __init__(self):
        self.ser = serial.Serial(SERIAL_PORT, BAUD, timeout=1)
        dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
        bus = dbus.SessionBus()
        obj = bus.get_object("org.navit_project.navit",
                             "/org/navit_project/navit/default_navit")
        iface = dbus.Interface(obj, dbus_interface="org.navit_project.navit")
        cb_path = iface.callback_attr_new()
        for attr in WATCHED:
            iface.add_attr(cb_path, attr)
        bus.add_signal_receiver(
            self.on_update,
            signal_name="callback_attr_update",
            dbus_interface="org.navit_project.navit",
            path=cb_path,
        )

    def on_update(self, attr_name, value):
        msg = None
        if attr_name == "navigation_next_turn":
            code = MANEUVER_MAP.get(str(value), str(value))
            msg = f"NAV:TURN:{code}\n"
        elif attr_name == "navigation_distance_turn":
            msg = f"NAV:DIST:{int(value)}\n"
        elif attr_name == "navigation_street_name":
            msg = f"NAV:STREET:{str(value)[:20]}\n"
        elif attr_name == "navigation_status":
            msg = f"NAV:STATUS:{str(value)}\n"
        elif attr_name == "eta":
            msg = f"NAV:ETA:{int(value)}\n"
        elif attr_name == "destination_length":
            msg = f"NAV:REMAIN:{int(value)}\n"
        elif attr_name == "position_direction":
            msg = f"NAV:HEAD:{int(value)}\n"
        elif attr_name == "position_time_iso8601":
            msg = f"NAV:TIME:{str(value)}\n"
        elif attr_name == "position_coord_geo":
            # value is typically (lng, lat) or struct; adapt to your D-Bus binding
            try:
                lat, lon = float(value[1]), float(value[0])  # (lng, lat) from Navit
                msg = f"NAV:POS:{lat:.4f},{lon:.4f}\n"
            except (IndexError, TypeError, ValueError):
                pass
        if msg:
            self.ser.write(msg.encode())

    def run(self):
        GLib.MainLoop().run()

if __name__ == "__main__":
    NavitBridge().run()
```

For full behaviour including eco icon when `eco_mode_fuel_enabled` is true, use **`tools/navit_dbus_to_pico_bridge.py`** (see above).

---

## 3. Serial Protocol (Host → Pico, 115200 baud, newline-terminated ASCII)

The same protocol is used regardless of whether the transport is USB CDC or Bluetooth SPP.

| Message | Meaning |
|---------|---------|
| `NAV:TURN:<code>` | Upcoming maneuver type. For eco icon (driver-break) see [3.1 Eco mode icon](#31-eco-mode-icon-driver-break-d-bus): use `eco_mode` or `eco` only when `eco_mode_fuel_enabled` is true and route is ready (see 3.1); it must **not** override routing instructions. |
| `NAV:DIST:<metres>` | Distance to next turn |
| `NAV:STREET:<n>` | Next street name (max 20 chars) |
| `NAV:STATUS:<status>` | Navit routing status |
| `NAV:ETA:<unix_ts>` | Estimated arrival (Unix timestamp); converted to local time and shown on clock line 2 as e.g. ARR14:32 |
| `NAV:REMAIN:<metres>` | Remaining distance to destination (from Navit `destination_length`); shown on clock line |
| `NAV:HEAD:<degrees>` | Heading 0-360 (from Navit `position_direction`); shown as compass on clock line |
| `NAV:TIME:<iso8601>` | GPS time UTC, e.g. `2026-03-14T12:34:56Z` (from Navit `position_time_iso8601`) |
| `NAV:POS:<lat>,<lon>` | GPS position in decimal degrees (from Navit `position_coord_geo`), e.g. `59.91,10.75` |
| `BT:TRACK:<info>` | Media track info from head unit |
| `BT:CALL:<caller>` | Incoming call caller ID |
| `BT:CALLEND` | Call ended |

### 3.1 Eco mode icon (driver-break, D-Bus)

The **eco mode** icon on the FIS is intended for the [Driver Break plugin](https://github.com/Supermagnum/navit/blob/feature/driver-break/docs/user/plugins/driver-break/index.rst) when fuel/energy is used in routing (ECU or adaptive fuel data). It must **never override routing instructions**: the host must only show the eco icon when appropriate and only for a short moment (e.g. after route planning finishes, before the first maneuver is displayed).

**Driver-break D-Bus attribute: `eco_mode_fuel_enabled`**

The plugin exposes a boolean attribute **eco_mode_fuel_enabled** on the Navit D-Bus interface. It is **true** when either (1) an ECU backend is available and running (OBD-II, J1939, or MegaSquirt), or (2) adaptive fuel learning is enabled in the plugin configuration. When **false**, no live ECU data and no adaptive learning are in use (energy-based routing may still be available without fuel data). See the [Driver Break D-Bus API (eco_mode_fuel_enabled)](https://github.com/Supermagnum/navit/blob/feature/driver-break/docs/user/plugins/driver-break/dbus.rst) for service, object path, interface, and examples in Python and `dbus-send`.

- **Service:** `org.navit_project.navit`
- **Method:** `get_attr("eco_mode_fuel_enabled")` on the navit object; returns `(attrname, value)` with a D-Bus boolean.

**When to show the eco icon**

- **`eco_mode_fuel_enabled` is true** (ECU running or adaptive fuel learning enabled).
- **Navit has planned or finished planning** a route that uses energy-based routing (fuel cost in the cost function).
- **Do not show eco when there is an active maneuver to display.** Example flow: when Navit finishes calculating a route and the above conditions are true, show the eco icon for a short period (e.g. 2–3 seconds), then send the actual first turn (`navigation_next_turn`) and distance so the FIS switches to normal turn-by-turn. The eco icon is an informational "route was planned with eco/fuel" hint, not a substitute for maneuver icons.

**Bridge script logic**

1. **Read the attribute** (e.g. on a timer or when `navigation_status` changes): call `get_attr("eco_mode_fuel_enabled")` on the navit object (see [dbus.rst](https://github.com/Supermagnum/navit/blob/feature/driver-break/docs/user/plugins/driver-break/dbus.rst) for object path and Python/dbus-send examples).
2. **Policy:** Send `NAV:TURN:eco_mode` to the Pico only when:
   - `eco_mode_fuel_enabled` is true, and
   - Route planning has just completed (e.g. `navigation_status` went to `routing`) or the route is ready and you are about to send the first maneuver.
3. Send `NAV:TURN:eco_mode` for a **short fixed duration** (e.g. 2–3 seconds), then immediately send the real first turn and distance (`NAV:TURN:<code>`, `NAV:DIST:<m>`) so the FIS shows the actual routing instruction. Never send or keep sending `eco_mode` when the user should see a turn icon (left, right, etc.).

**Firmware behaviour:** The Pico displays whatever `NAV:TURN` code the host last sent. The **host is responsible** for never letting eco_mode override a maneuver: the bridge must only send `NAV:TURN:eco_mode` in the narrow window described above and then replace it with the real maneuver.

**Summary:** Eco icon = show only when `eco_mode_fuel_enabled` is true and route planning has just finished; display it briefly, then always switch to the real first (and subsequent) routing instructions.

**Feature toggles (clock screen):** Send `CFG:<name>:0` or `CFG:<name>:1` to enable or disable what is shown. Clock/ETA/compass/remain default to 1 (on). CAN bus has **full firmware support** (send/receive, 100 kbit/s, MCP2561) but is **disabled by default** (0).

| Message | Effect |
|---------|--------|
| `CFG:CLOCK:0` / `CFG:CLOCK:1` | Clock (time from GPS, set automatically); 0 = do not show clock on FIS |
| `CFG:ETA:0` / `CFG:ETA:1` | ETA in local time on clock line 2 (e.g. ARR14:32) |
| `CFG:COMPASS:0` / `CFG:COMPASS:1` | Compass heading (N, NE, ...) on clock line 2 |
| `CFG:REMAIN:0` / `CFG:REMAIN:1` | Remaining distance on clock line 2 |
| `CFG:CAN:0` / `CFG:CAN:1` | CAN bus (full support: send/receive at 100 kbit/s, **disabled by default**). Enable with 1 when MCP2561 is connected to GPIO 11/12. |

---

## 4. Transport — USB CDC or Bluetooth SPP

The host device can connect to the Pico over either transport. The Pico listens on both
USB CDC and Bluetooth SPP simultaneously.

### USB CDC
- Connect a USB cable from the host to the Pico USB port
- Pico appears as `/dev/ttyACM0` (Linux) or equivalent on Android
- **Also provides 5 V power to the Pico via VSYS** — no separate power supply needed
- Baud rate: 115200

### Bluetooth SPP
- Pico advertises as `FIS-Bridge` using classic Bluetooth SPP via BTstack (onboard CYW43439)
- No external Bluetooth module required
- **Pair once:** scan for `FIS-Bridge`, enter PIN `0000` if prompted — reconnects automatically
- On Linux: Pico appears as `/dev/rfcomm0` after pairing
- Baud rate: 115200

> If using Bluetooth only (no USB), the Pico needs a separate 5 V power source via the VSYS
> pin — e.g. a car USB adapter or a 12 V → 5 V regulator on the PCB.

---

## 5. Hardware

### 5.1 Controller — Raspberry Pi Pico 2 W

| Property | Value |
|----------|-------|
| MCU | RP2350 (dual-core Cortex-M33, 150 MHz) |
| RAM | 520 KB SRAM |
| Flash | 4 MB |
| Wireless | Infineon CYW43439 — WiFi + classic Bluetooth |
| Logic level | 3.3 V GPIO |
| Power | 5 V via VSYS (USB or external) |
| DigiKey P/N | SC0919 |

### 5.2 The 3LB (Three-Line Bus)

| Wire | Function | Direction |
|------|----------|-----------|
| ENA | Enable / bus arbitration | Bidirectional |
| DATA | Serial data | Master → Cluster |
| CLK | Clock | Master → Cluster |

Bus runs at **5 V logic**,  2300 Hz,Pico GPIO is 3.3 V — level shifting required on all lines.

**Bus arbitration:** Before transmitting, a master raises ENA to claim the bus. If ENA is already
high it waits. The cluster acknowledges with a 100 µs pulse. The Pico and ECU safely share the
bus without any relay or switch.

### 5.3 Level Shifting

6× **BS170** N-channel MOSFET (TO-92, through-hole), one per 3LB GPIO line (3 RX + 3 TX).
Each channel uses two 2.2 kΩ pull-up resistors — one to 3.3 V, one to 5 V. Use non-inductive resistors (e.g. metal film or SMD thick/thin film); avoid wirewound types.

| Part | DigiKey P/N | Qty |
|------|-------------|-----|
| BS170 MOSFET TO-92 THT | BS170-ND | 6 |
| 2.2 kΩ resistor 0.25 W THT (non-inductive) | — | 12 |

BS170 pinout (flat face toward you, left to right): **Source — Gate — Drain**.

### 5.4 Instrument Cluster Connector (Green T32a)

| T32a Pin | Signal |
|----------|--------|
| 30 | DATA |
| 31 | CLK |
| 32 | ENA |

Connector type: Kostal MLK 1.2 / MQS 0.63 mm. Use a VAG pin tool to back-probe terminals.
Do not cut harness wires unless you are absolutely sure what you are doing.
One option is to cut the harness wires needed and add connectors so you can plug the PCB in line; then you can unplug both sides and connect them directly to bypass the board for repairs or to restore original function. Use the Molex 3-circuit kit (see below) for pigtails from the PCB or for a pure wire-to-wire connection.

### 5.5 3LB connection: pigtails or wire-to-wire (Molex kit)

No PCB-mounted connector. Use one **Molex 3-circuit connector kit** (DigiKey **23-0766500064-ND**, Molex **0766500064**, "KIT CONN STD .062" 3 CIRCUITS"). The kit is the complete solution for both PCB pigtails and wire-to-wire.

**With PCB (pigtails):** Solder two short pigtail wires directly to the PCB RX and TX pad areas. On the free end of each pigtail, crimp the correct connector from the kit:

| Side | PCB pad | Free end (from kit) |
|------|---------|---------------------|
| **RX** | Solder wire to PCB RX pads | Female receptacle housing **0003061038** with socket terminals **0002061103** |
| **TX** | Solder wire to PCB TX pads | Plug housing **0003062033** with pin terminals **0002062103** |

Incoming field cables then plug into those pigtails — female into male, male into female. The different housing genders prevent swapping RX and TX.

**Wire-to-wire (no PCB):** To connect two cables with no board in between, plug the kit female receptacle housing directly into the male plug housing. No PCB, no soldering, no extra parts.

| Part | DigiKey P/N | Qty |
|------|-------------|-----|
| Molex 3-circuit connector kit (.062" / 1.57 mm) | 23-0766500064-ND | 1 |

### 5.6 Pico 2 W GPIO Pinout

**3LB RX — monitor ECU frames, detect bus idle (inputs, via level shifter):**

| GPIO | Signal |
|------|--------|
| GPIO0 | FIS_PIN_ENA — monitors bus idle state |
| GPIO1 | FIS_PIN_CLK — input to PIO SM0 |
| GPIO2 | FIS_PIN_DATA — input to PIO SM0 |

**3LB TX — inject frames during idle gaps (outputs, via level shifter):**

| GPIO | Signal |
|------|--------|
| GPIO3 | FIS_PIN_ENA_OUT — claims bus before injecting |
| GPIO4 | FIS_PIN_CLK_OUT — PIO SM1 side-set |
| GPIO5 | FIS_PIN_DATA_OUT — PIO SM1 out_base |

**Optional CAN (firmware supports full CAN; when enabled, MCP2561):** On the VW Passat B6 (PQ46 platform) the bus connecting the instrument cluster to the rest of the modules is the **Komfort-CAN** (Comfort CAN), also referred to as **K-CAN** or **KCAN**. The CAN transceiver used is the MCP2561. It has only 2 signal connections: TXD and RXD. 3.3 V logic; no level shifter needed. See `firmware/fis_can.h` and firmware README.

| GPIO   | Signal         | Direction | Notes                    |
|--------|----------------|-----------|---------------------------|
| GPIO11 | FIS_CAN_PIN_TX | Out       | TX CAN to MCP2561 TXD     |
| GPIO12 | FIS_CAN_PIN_RX | In        | RX CAN from MCP2561 RXD   |

MCP2561 CANH/CANL connect to the vehicle Komfort-CAN (Comfort CAN / K-CAN, 100 kbit/s). For when to add a 120 ohm termination (tapped in middle vs replacing radio vs bench), see firmware README.

**PQ46 CAN buses:** The Komfort-CAN is distinct from the other two CAN buses on the platform:

| Bus | Bit rate | Typical traffic |
|-----|----------|------------------|
| **Komfort-CAN** (K-CAN) | 100 kbit/s | Instrument cluster, convenience modules, gateway; messages like mKombi_1, mDiagnose_1 |
| **Antriebs-CAN** (ACAN / Drivetrain CAN) | 500 kbit/s | Engine, ABS, airbag, gearbox |
| **Infotainment-CAN** (ICAN) | 100 kbit/s | Radio, navigation, phone module |

The **CAN gateway (J533)** sits between these buses and bridges messages as needed. When your Pico (via MCP2561) taps into the bus to read or send messages such as mKombi_1, mDiagnose_1, etc., it is connecting to the **Komfort-CAN** (K-CAN), not to ACAN or ICAN.

### 5.7 Cluster Coding Prerequisite

Code the cluster using **VCDS** or **OBDeleven**, Module **17 — Instruments**:
enable **"Navigation present"**. Without this the navigation slot in the FIS/MFA is inactive
even if the Pico sends correct 3LB frames.

---

## 6. The 3LB Protocol

### 6.1 Open Source Libraries (Reference)

| Library | URL | Notes |
|---------|-----|-------|
| TLBFISLib | https://github.com/domnulvlad/TLBFISLib | Most complete, actively maintained |
| VAGFISWriter | https://github.com/tomaskovacik/VAGFISWriter | Original reverse-engineered implementation |
| FISCuntrol | https://github.com/adamforbes92/FISCuntrol | Complete project, author uses in own car |
| FISBlocks | https://github.com/ibanezgomez/FISBlocks | 3LB + KWP1281 OBD combined |

The Pico firmware reimplements 3LB directly in PIO state machines timed to the BAP FCNav
SDP30DF48V280F specification — no Arduino library dependency.

### 6.2 Display

The FIS/MFA (Fahrerinformationssystem / Multifunktionsanzeige) screen is a **64×88 pixel
monochrome LCD**, 1-bit. Navigation maneuver icons are pre-generated 64×64 px 1-bit arrays
stored in flash (`firmware/fis_nav_icons.h`) and rendered with:
```c
GraphicFromArray(x, y, width, height, array, 0); // 0 = flash/PROGMEM
```

### 6.3 OEM radio: CAN time and display (reference only)

The **original VW radio** set the instrument cluster time via the **CAN bus** (CAN-H / CAN-L), using time it received from the **FM RDS network**. This project does **not** use CAN for the clock; the Pico sets the FIS clock from GPS (Navit) over serial and 3LB only. The following is documented for reference.

**OEM messages implemented:** When CAN is enabled (`CFG:CAN:1`), the firmware sends **mDiagnose_1** (0x7D0) with local date/time derived from GPS (NAV:TIME + position for timezone) every 1000 ms, and **mEinheiten** (0x60E) with display format (24 h, EU date, units). The cluster can use mDiagnose_1 to set its internal clock. The FIS clock display is still driven by the serial protocol and 3LB injection; mDiagnose_1 allows the cluster’s own time display to stay in sync when CAN is connected.

| Message | CAN ID | Length | Period | Description |
|---------|--------|--------|--------|-------------|
| **mDiagnose_1** | 0x7D0 (2000) | 8 bytes | 1000 ms | Date and time from radio (e.g. RDS). |
| **mEinheiten** | 0x60E (1550) | 2 bytes | 1000 ms | Display format flags (date/clock on-off, day of week). |

**mDiagnose_1 (0x7D0) — date and time**

| Signal | Byte | Start bit | Length | Range | Unit |
|--------|------|------------|--------|-------|------|
| DI1_Jahr (Year) | 4 | 4 | 7 bits | 0–127 → 2000–2127 | Year |
| DI1_Monat (Month) | 5 | 3 | 4 bits | 1–12 | Month |
| DI1_Tag (Day) | 5 | 7 | 5 bits | 0–31 | Day |
| DI1_Stunde (Hour) | 6 | 4 | 5 bits | 0–23 | Hours |
| DI1_Minute (Minute) | 7 | 1 | 6 bits | 0–59 | Minutes |
| DI1_Sekunde (Second) | 7 | 7 | 6 bits | 0–59 | Seconds |
| DI1_Zeit_alt (time stale) | 8 | 7 | 1 bit | — | 1 = time old/stale |

Sent cyclically every 1000 ms.

**mEinheiten (0x60E) — display format**

| Signal | Byte | Bit(s) | Description |
|--------|------|--------|-------------|
| EH1_Datum_Anzeige | 1 | 6 | Date display on/off |
| EH1_Uhr_Anzeige | 1 | 7 | Clock display on/off |
| EH1_Wochentag | 2 | 4–6 | Day of week |

Sent cyclically every 1000 ms.

### 6.4 Other potentially useful CAN messages (reference only)

**mDiagnose_1** and **mEinheiten** are implemented and sent when CAN is enabled (see 6.3). The following CAN frames are not yet implemented; they are documented for reference. You can add handling for these or other IDs using `fis_can_send()` / `fis_can_receive()`.

**Useful for FIS/display**

- **mKombi_1 (0x320)** — sent by the cluster, 10 ms cyclic: KO1_kmh (vehicle speed, bytes 4–5, 15 bits), KO1_angez_kmh (displayed speed including offset, bytes 6–7), KO1_Tankinhalt (fuel level), KO1_Warn_Tank (low fuel, 7 L), KO1_Oeldruck (oil pressure warning), KO1_Kuehlmittel (coolant warning), KO1_Vorgluehen (glow plug, diesel).
- **mKombi_2 (0x420)** — from cluster, 200 ms cyclic: KO2_gef_Auss_T / KO2_Aussen_T (outside temperature), KO2_Bel_Displ (display brightness %, for dimming FIS at night), KO2_Fehlereintr (fault stored).
- **mEinheiten (0x60E)** — units: EH1_Uhr_Anzeige (12h/24h), EH1_Datum_Anzeige (EU/US date), EH1_Wochentag (day of week 1=Mon…7=Sun), EH1_Einh_Temp (°C/°F), EH1_Einh_Strck (km/miles).

**Situational awareness**

- **mGate_Komf_1 (0x390)**, 100 ms: GK1_Rueckfahr (reverse), GK1_Warnblk_Status (hazards), GK1_Bremslicht, GK1_Abblendlicht / GK1_Fernlicht (headlights/high beam), GK1_Wischer_vorn (wipers), GK1_SH_laeuft (aux heater), door status (driver, passenger, rear L/R).
- **mZAS_1 (0x572)** — ignition: ZA1_Klemme_15 (ignition on), ZA1_Klemme_50 (starter), ZA1_S_Kontakt (key in).
- **mClima_1 (0x5E0)** — CL1_Kompressor (A/C on), CL1_Hecksch / CL1_Frontsch (rear/front screen heat), CL1_Restwaerme (residual heat).

**Parking (if equipped)**

- **Parkhilfe_01 (0x497)** — PDC: obstacle front/rear, beeper active, audio ducking request, system state.

**Other**

- **mKombi_3 (0x520)** — KO3_Kilometer (odometer), KO3_Standzeit (parked duration).
- **mSysteminfo_1 (0x5D0)** — SY1_Fzg_Derivat (body style), SY1_Notbrems_Status (emergency braking).

**Signal reference tables**

*mKombi_1 — 0x320 (from cluster, 10 ms cyclic)*

| Signal | Byte | Start bit | Len | Raw range | Physical | Unit | Scale |
|--------|------|-----------|-----|-----------|----------|------|-------|
| KO1_kmh | 4 | 1 | 15 | 0–32600 | 0–326 | km/h | 0.01 |
| KO1_angez_kmh | 6 | 6 | 10 | 0–1018 | 0–325.76 | km/h | 0.32 |
| KO1_Tankinhalt | 3 | 0 | 7 | 0–126 | 0–126 | litres | 1 |
| KO1_Warn_Tank | 1 | 6 | 1 | — | 1 = warning (7 L) | — | — |
| KO1_Oeldruck | 1 | 2 | 1 | — | 1 = low pressure | — | — |
| KO1_Kuehlmittel | 1 | 4 | 1 | — | 1 = coolant low | — | — |
| KO1_Vorgluehen | 1 | 7 | 1 | — | 1 = glow plug on | — | — |

KO1_angez_kmh spans bytes 6–7 (10 bits from bit 6); KO1_kmh spans bytes 4–5 (15 bits from bit 1). Little-endian.

*mKombi_2 — 0x420 (from cluster, 200 ms cyclic)*

| Signal | Byte | Start bit | Len | Raw range | Physical | Unit | Offset | Scale |
|--------|------|-----------|-----|-----------|----------|------|--------|-------|
| KO2_gef_Auss_T | 2 | 0 | 8 | 0–254 | −50 to +77 | °C | −50 | 0.5 |
| KO2_Aussen_T | 3 | 0 | 8 | 0–254 | −50 to +77 | °C | −50 | 0.5 |
| KO2_Bel_Displ | 6 | 0 | 7 | 0–100 | 0–100 | % | 0 | 1 |
| KO2_Fehlereintr | 1 | 7 | 1 | — | 1 = fault stored | — | — | — |

Temperature: °C = (raw × 0.5) − 50. Raw 0xFF = invalid.

*mEinheiten — 0x60E (1000 ms cyclic)*

| Signal | Byte | Start bit | Len | Values |
|--------|------|-----------|-----|--------|
| EH1_Uhr_Anzeige | 1 | 7 | 1 | 0 = 24 h, 1 = 12 h |
| EH1_Datum_Anzeige | 1 | 6 | 1 | 0 = EU (DD.MM), 1 = US (MM/DD) |
| EH1_Wochentag | 2 | 4 | 3 | 0 = init, 1 = Mon … 7 = Sun |
| EH1_Einh_Temp | 1 | 1 | 1 | 0 = °C, 1 = °F |
| EH1_Einh_Strck | 1 | 0 | 1 | 0 = km, 1 = miles |

*mGate_Komf_1 — 0x390 (100 ms cyclic)*

| Signal | Byte | Start bit | Len | Notes |
|--------|------|-----------|-----|-------|
| GK1_Rueckfahr | 4 | 4 | 1 | 1 = reverse |
| GK1_Warnblk_Status | 7 | 7 | 1 | 1 = hazards on |
| GK1_Bremslicht | 8 | 3 | 1 | 1 = brake light on |
| GK1_Abblendlicht | 7 | 0 | 1 | 1 = dipped beam |
| GK1_Fernlicht | 7 | 1 | 1 | 1 = high beam |
| GK1_Wischer_vorn | 7 | 2 | 1 | 1 = wipers moving |
| GK1_SH_laeuft | 8 | 0 | 1 | 1 = aux heater on |
| GK1_Fa_Tuerkont | 3 | 0 | 1 | Driver door |
| BSK_BT_geoeffnet | 6 | 1 | 1 | Passenger door |
| BSK_HL_geoeffnet | 4 | 2 | 1 | Rear left door |
| BSK_HR_geoeffnet | 4 | 3 | 1 | Rear right door |

*mZAS_1 — 0x572 (ignition/key)*

| Signal | Byte | Start bit | Len | Notes |
|--------|------|-----------|-----|-------|
| ZA1_S_Kontakt | 1 | 0 | 1 | 1 = key inserted |
| ZA1_Klemme_15 | 1 | 1 | 1 | 1 = ignition on |
| ZA1_Klemme_50 | 1 | 3 | 1 | 1 = starter engaged |

*mClima_1 — 0x5E0*

| Signal | Byte | Start bit | Len | Notes |
|--------|------|-----------|-----|-------|
| CL1_Kompressor | 1 | 4 | 1 | 1 = A/C on |
| CL1_Hecksch | 1 | 2 | 1 | 1 = rear screen heat |
| CL1_Frontsch | 1 | 3 | 1 | 1 = front screen heat |
| CL1_Restwaerme | 7 | 3 | 1 | 1 = residual heat on |

*Parkhilfe_01 — 0x497 (if equipped)*

| Signal | Byte | Start bit | Len | Notes |
|--------|------|-----------|-----|-------|
| PH_Opt_Anz_V_Hindernis | 3 | 2 | 1 | 1 = obstacle front |
| PH_Opt_Anz_H_Hindernis | 3 | 3 | 1 | 1 = obstacle rear |
| PH_Tongeber_V_aktiv | 3 | 4 | 1 | 1 = PDC beep front |
| PH_Tongeber_H_aktiv | 3 | 5 | 1 | 1 = PDC beep rear |
| PH_Anf_Audioabsenkung | 3 | 7 | 1 | 1 = audio duck request |
| PH_Systemzustand | 8 | 2 | 3 | 0 = off, 1 = reverse, 2 = front, 3 = both, 7 = fault |

*mKombi_3 — 0x520*

| Signal | Byte | Start bit | Len | Raw range | Physical | Unit | Scale |
|--------|------|-----------|-----|-----------|----------|------|-------|
| KO3_Kilometer | 6 | 0 | 20 | 0–1048575 | 0–1048575 | km | 1 |
| KO3_Standzeit | 4 | 0 | 15 | 0–32767 | 0–131068 | seconds | ×4 |

KO3_Standzeit = time since last ignition-off in 4-second steps (max ~36.4 h).

*mSysteminfo_1 — 0x5D0*

| Signal | Byte | Start bit | Len | Values |
|--------|------|-----------|-----|--------|
| SY1_Fzg_Derivat | 3 | 0 | 4 | 0 = saloon, 1 = notchback, 2 = estate, 3 = hatchback, 4 = coupé, 5 = cabriolet, 6 = offroad, 9 = other |
| SY1_Notbrems_Status | 8 | 0 | 1 | 1 = emergency braking |

---

## 7. Firmware Runtime Behaviour

**Core 0:**
- Initialises USB CDC and BTstack SPP (`FIS-Bridge`, PIN `0000`)
- Reads `NAV:*`, `BT:*`, and `CFG:*` messages from both interfaces non-blocking
- Updates shared `nav_state_t` and feature toggles (`fis_config_t`) under a critical section
- When CAN is enabled (`CFG:CAN:1`), runs CAN poll (firmware has full CAN support: send/receive at 100 kbit/s via MCP2561 on GPIO 11/12)

**Core 1:**
- Monitors 3LB ENA/CLK/DATA via PIO SM0 (RX sniffer)
- Detects idle gaps between ECU transmissions
- When bus is idle, checks `nav_state_t` and feature toggles (`fis_config_t`), then injects:
  - Active call → call frame (highest priority)
  - Active navigation (`routing` / `recalculating`) → nav icon + street name + distance
  - Media info present, no nav → track name frame
  - Clock enabled and GPS time available → clock frame (local time on line 1; line 2: remain / ETA / compass / date per toggles)
  - Otherwise → do nothing; ECU frames reach FIS/MFA unmodified

**Injection:** Wait ENA LOW (bus idle) → raise ENA → transmit via PIO SM1 → lower ENA → ECU resumes.

---

## 8. Software Stack Summary

| Layer | Technology | Where it runs |
|-------|-----------|---------------|
| Navigation engine | Navit (with `libbinding_dbus.so`) | Host device (Android, Linux, etc.) |
| D-Bus listener + serial bridge | Python 3 (`dbus`, `pyserial`) | Host device |
| Serial transport | USB CDC or Bluetooth SPP | Host ↔ Pico |
| Bus monitor + arbitration | PIO SM0 (3LB RX) | Pico 2 W |
| FIS/MFA frame injector | PIO SM1 (3LB TX) | Pico 2 W |
| Bluetooth receiver | BTstack SPP (CYW43439) | Pico 2 W |
| FIS/MFA display | 3LB hardware (ENA/DATA/CLK) | Pico → Cluster |

---

## 9. Key References

| Resource | URL |
|----------|-----|
| PQ35/46 CAN K-Matrix (VW/Audi) | https://github.com/Supermagnum/FIS-Display/blob/main/PQ35_46_ACAN_KMatrix_V5.20.6F_20160530_MH.xlsx |
| PQ35/46 CAN Glossary (DE–EN) | https://github.com/Supermagnum/FIS-Display/blob/main/PQ35_46_ACAN_Glossary_DE_EN.md |
| Navit D-Bus external dashboard spec | https://github.com/Supermagnum/navit/blob/feature/navit-dash/docs/development/navit_dbus_external_dashboards.rst |
| Navit driver-break plugin (index) | https://github.com/Supermagnum/navit/blob/feature/driver-break/docs/user/plugins/driver-break/index.rst |
| Navit driver-break D-Bus API (eco_mode_fuel_enabled) | https://github.com/Supermagnum/navit/blob/feature/driver-break/docs/user/plugins/driver-break/dbus.rst |
| Navit project | https://github.com/navit-gps/navit |
| TLBFISLib | https://github.com/domnulvlad/TLBFISLib |
| VAGFISWriter | https://github.com/tomaskovacik/VAGFISWriter |
| VAGFISReader theory of operation | https://github.com/tomaskovacik/VAGFISReader/wiki/Theory-of-operation |
| FISCuntrol | https://github.com/adamforbes92/FISCuntrol |
| FISBlocks | https://github.com/ibanezgomez/FISBlocks |
| FIS-Fool intercept concept | http://kuni.bplaced.net/_Homepage/FisFool/FisFool_Overview_e.html |
| BAP FCNav SDP30DF48V280F (3LB timing spec) | https://github.com/Supermagnum/FIS-Display/blob/main/pdfcoffee.com_bap-fcnav-sdp30df48v280fpdf-pdf-free.pdf |
| SVG maneuver icons | https://github.com/Supermagnum/navit/tree/feature/navit-dash/docs/development/svg-examples |
| Raspberry Pi Pico 2 W datasheet | https://datasheets.raspberrypi.com/picow/pico-2-w-datasheet.pdf |

---

*Platform: VW Passat B6 (3C), 2005–2010, PQ46 platform.
Cluster part numbers: 3C0 920 8xx series.
FIS/MFA screen: 64×88 px monochrome LCD, 1-bit, 3LB protocol.*
