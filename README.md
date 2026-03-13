# FIS-Display
VW Passat B6 FIS/MFA Display — Hardware Connections and Software Protocols

## For Navit D-Bus Navigation + Media/Call Integration via Raspberry Pi Pico 2 W

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
| `eta` | Estimated arrival (Unix timestamp) |
| `destination_length` | Remaining distance (m) |

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
    "destination_length",
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
| `NAV:ETA:<unix_ts>` | Estimated arrival time |
| `NAV:REMAIN:<metres>` | Total remaining distance |
| `BT:TRACK:<info>` | Media track info from head unit |
| `BT:CALL:<caller>` | Incoming call caller ID |
| `BT:CALLEND` | Call ended |

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

Bus runs at **5 V logic**,  2500 kHz. Pico GPIO is 3.3 V — level shifting required on all lines.

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

---

## 7. Firmware Runtime Behaviour

**Core 0:**
- Initialises USB CDC and BTstack SPP (`FIS-Bridge`, PIN `0000`)
- Reads `NAV:*` and `BT:*` messages from both interfaces non-blocking
- Updates shared `nav_state_t` protected by a mutex

**Core 1:**
- Monitors 3LB ENA/CLK/DATA via PIO SM0 (RX sniffer)
- Detects idle gaps between ECU transmissions
- When bus is idle, checks `nav_state_t` and injects:
  - Active call → call frame (highest priority)
  - Active navigation (`routing` / `recalculating`) → nav icon + street name + distance
  - Media info present, no nav → track name frame
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
