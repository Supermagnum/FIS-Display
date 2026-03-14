# FIS-Display
VW Passat B6 FIS/MFA Display — Hardware Connections and Software Protocols.

The 3LB bus runs at approximately 2300 Hz (2.3 kHz). The 3LB is a synchronous serial bus with a clock of ~2300 Hz, and it sends 1 bit per clock cycle, so that's:
~2300 bits per second (2.3 kbps).

No circuit board has been made yet, but it will be.

## For Navit D-Bus Navigation + Media/Call Integration via Raspberry Pi Pico 2 W

> **Note:** The Pico 2 W firmware in `firmware/` is **experimental and untested on a real vehicle**. Validate on the bench before connecting to the car. See `firmware/README.md` for build and flash instructions.

PCB files and schematic diagram will be made soon.

---

## Table of contents

- [1. System Overview](#1-system-overview)
- [2. Host Device — Navit with D-Bus](#2-host-device--navit-with-d-bus)
  - [2.1 Supported Platforms](#21-supported-platforms)
  - [2.2 D-Bus Service Details](#22-d-bus-service-details)
  - [2.3 Key Attributes to Subscribe To](#23-key-attributes-to-subscribe-to)
  - [2.4 Navigation Status Values](#24-navigation-status-values)
  - [2.5 D-Bus Listener to Pico Bridge Script](#25-d-bus-listener--pico-bridge-script)
- [3. Serial Protocol (Host to Pico, 115200 baud)](#3-serial-protocol-host--pico-115200-baud-newline-terminated-ascii)
- [4. Transport — USB CDC or Bluetooth SPP](#4-transport--usb-cdc-or-bluetooth-spp)
  - [USB CDC](#usb-cdc)
  - [Bluetooth SPP](#bluetooth-spp)
- [5. Hardware](#5-hardware)
  - [5.1 Controller — Raspberry Pi Pico 2 W](#51-controller--raspberry-pi-pico-2-w)
  - [5.2 The 3LB (Three-Line Bus)](#52-the-3lb-three-line-bus)
  - [5.3 Level Shifting](#53-level-shifting)
  - [5.4 Instrument Cluster Connector (Green T32a)](#54-instrument-cluster-connector-green-t32a)
  - [5.5 Inline PCB Connector](#55-inline-pcb-connector)
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
               │ Middleman only:  │
               │ receives serial  │
               │ protocol, injects│
               │ onto 3LB bus     │
               └────────┬─────────┘
                        │
             3LB (ENA / CLK / DATA)
             via BS170 level shifters
             (3.3V ↔ 5V)
                        │
          ┌─────────────┴──────────────┐
          │                            │
          ▼                            ▼
┌──────────────────────┐   ┌───────────────────────┐
│ VW Passat B6 (3C)    │   │ Original ECU           │
│ FIS/MFA instrument   │◄──│ (talks 3LB natively,   │
│ cluster LCD screen   │   │  Pico co-exists via    │
│ 64×88 px, 1-bit      │   │  ENA arbitration)      │
└──────────────────────┘   └───────────────────────┘
```

The Pico 2 W is a **pure middleman**. It has no navigation intelligence — it only receives
the serial protocol from the host device and injects the translated frames onto the 3LB bus.
All navigation logic stays on the host device running Navit.

The original ECU continues to talk to the FIS/MFA natively over 3LB at all times. The Pico
co-exists on the bus using the ENA line for arbitration — no relay or analog switch is needed.

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

### 2.5 D-Bus Listener → Pico Bridge Script

This script runs on the host device and forwards Navit D-Bus signals to the Pico over either
USB CDC or Bluetooth SPP. Configure `SERIAL_PORT` to match your transport:

- USB CDC: typically `/dev/ttyACM0` (Linux/Android)
- Bluetooth SPP: the rfcomm device created when paired to `FIS-Bridge`, e.g. `/dev/rfcomm0`

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

---

## 3. Serial Protocol (Host → Pico, 115200 baud, newline-terminated ASCII)

The same protocol is used regardless of whether the transport is USB CDC or Bluetooth SPP.

| Message | Meaning |
|---------|---------|
| `NAV:TURN:<code>` | Upcoming maneuver type |
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

**Feature toggles (clock screen):** Send `CFG:<name>:0` or `CFG:<name>:1` to enable or disable what is shown. All default to 1 (on).

| Message | Effect |
|---------|--------|
| `CFG:CLOCK:0` / `CFG:CLOCK:1` | Clock (time from GPS, set automatically); 0 = do not show clock on FIS |
| `CFG:ETA:0` / `CFG:ETA:1` | ETA in local time on clock line 2 (e.g. ARR14:32) |
| `CFG:COMPASS:0` / `CFG:COMPASS:1` | Compass heading (N, NE, ...) on clock line 2 |
| `CFG:REMAIN:0` / `CFG:REMAIN:1` | Remaining distance on clock line 2 |

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
Each channel uses two 2.2 kΩ pull-up resistors — one to 3.3 V, one to 5 V. Use metal film or SMD thick/thin film resistors for low parasitic inductance; avoid wirewound types.

| Part | DigiKey P/N | Qty |
|------|-------------|-----|
| BS170 MOSFET TO-92 THT | BS170-ND | 6 |
| 2.2 kΩ resistor 0.25 W THT | — | 12 |

BS170 pinout (flat face toward you, left to right): **Source — Gate — Drain**.

### 5.4 Instrument Cluster Connector (Green T32a)

| T32a Pin | Signal |
|----------|--------|
| 30 | DATA |
| 31 | CLK |
| 32 | ENA |

Connector type: Kostal MLK 1.2 / MQS 0.63 mm. Use a VAG pin tool to back-probe terminals.
Do not cut harness wires unless you are absolutely sure what you are doing.
One can cut the wires needed, add female and male that can connect together or to ECU and gauge cluster side connectors on the PCBs headers. Then it easy to remove the pcb for repairs and restore original function.

### 5.5 Inline PCB Connector

JST PH 2.00 mm pitch, 3-position, one pair each side. Makes the PCB fully removable —
unplug both sides and connect them directly to bypass the board.

| Part | DigiKey P/N | Qty |
|------|-------------|-----|
| JST PHR-3 receptacle housing | 455-1705-ND | 2 |
| JST B3B-PH-K PCB header (0.079"/2.00 mm pitch, single row) | 455-1126-ND | 2 |
| JST SPH-002T-P0.5S crimp terminals (24–32 AWG) | 455-2148TR-ND | 6 |

Any 3-position single-row 2.00 mm pitch THT header is a suitable alternative for the PCB header.

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

The **original VW radio** set the instrument cluster time via the **CAN bus** (CAN-H / CAN-L), using time it received from the **FM RDS network**. This project does **not** use CAN; the Pico sets the FIS clock from GPS (Navit) over serial and 3LB only. The following is documented for reference.

**Not implemented:** The CAN messages below are **not** implemented in the Pico firmware and there is **no CAN transceiver or connector on the current PCB**. The FIS clock is driven solely by the serial protocol (NAV:TIME, timezone conversion, and 3LB injection) described earlier.

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

The following CAN frames are **not** implemented in the firmware or on the PCB. They are documented for reference if CAN is added later (e.g. to read cluster/vehicle state or adapt FIS output).

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
| Navit D-Bus external dashboard spec | https://github.com/Supermagnum/navit/blob/feature/navit-dash/docs/development/navit_dbus_external_dashboards.rst |
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
