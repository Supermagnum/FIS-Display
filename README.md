# FIS-Display
VW Passat B6 FIS Display — Hardware Connections and Software Protocols

# VW Passat B6 FIS Display — Hardware Connections and Software Protocols
## For Navit D-Bus Navigation + Android Radio Integration

---

## 1. System Overview

```
┌─────────────────────┐        Serial / USB        ┌──────────────────────────┐
│  Raspberry Pi        │ ─────────────────────────► │                          │
│  running Navit       │   nav data (text/JSON)     │   Arduino Nano / ESP32   │
│  + D-Bus listener    │                            │   (middleman controller) │
└─────────────────────┘                             │                          │
                                                    │                          │
┌─────────────────────┐        Bluetooth            │                          │
│  Android head unit   │ ─────────────────────────► │                          │
│  (radio / AVRCP)     │   track name, call ID      │                          │
└─────────────────────┘                             └──────────┬───────────────┘
                                                               │
                                                        3LB (3-wire bus)
                                                        ENA / DATA / CLK
                                                               │
                                                               ▼
                                                  ┌────────────────────────┐
                                                  │  VW Passat B6 (3C)     │
                                                  │  FIS instrument cluster │
                                                  │  monochrome LCD screen  │
                                                  └────────────────────────┘
```

The Arduino/ESP32 is the bridge. It receives navigation data from the Pi (Navit via D-Bus) and media/call info from the Android radio (Bluetooth AVRCP), then writes formatted text to the FIS screen using the 3LB protocol.

---

## 2. VW Passat B6 FIS Hardware Connections

### 2.1 The 3LB (Three-Line Bus) — What It Is

The FIS screen in the Passat B6 cluster communicates with the OEM radio/navigation unit via a **3-wire serial bus** called **3LB** (also known as TLB or TWB). It carries:

| Wire  | Function                  | Direction         |
|-------|---------------------------|-------------------|
| ENA   | Enable / chip select      | Bidirectional     |
| DATA  | Serial data               | Radio → Cluster   |
| CLK   | Clock                     | Radio → Cluster   |

These three pins are present on the **back of the OEM radio** (mini-ISO connector, middle section) and also accessible directly on the **green T32a connector** on the back of the instrument cluster.

> **Note:** Your Android head unit almost certainly does NOT use this bus — most aftermarket Android radios ignore it entirely. This means there is no conflict and the Arduino can take full ownership of the 3LB.

---

### 2.2 Instrument Cluster Connector Pinout (Green T32a)

The relevant 3LB pins on the green connector (T32a) of the Passat B6 cluster:

| T32a Pin | Signal | Notes                          |
|----------|--------|--------------------------------|
| 30       | DATA   | Serial data from radio/Arduino |
| 31       | CLK    | Clock from radio/Arduino       |
| 32       | ENA    | Enable, bidirectional          |

> Confirm pin numbers against your specific cluster part number. The Passat B6 (3C) uses part numbers in the `3C0 920 8xx` range. Connector colours and pinouts can be verified with VCDS/OBDeleven or the VAG wiring documentation.

---

### 2.3 Arduino Nano Wiring to FIS Cluster

```
Arduino Nano                    Cluster T32a connector
─────────────                   ──────────────────────
Pin D2    ──────────────────►   Pin 32  (ENA)
Pin D6    ──────────────────►   Pin 30  (DATA)
Pin D7    ──────────────────►   Pin 31  (CLK)
GND       ──────────────────►   Cluster GND

Power: Arduino powered from car 12V via a 7805 or buck converter to 5V.
No level shifting needed — the cluster runs at 5V logic on these pins.
```

> If using an **ESP32** instead of Arduino Nano, the ESP32 is 3.3V logic. You will need a **level shifter** (e.g. TXS0108E or BSS138-based) on all three lines before connecting to the cluster.

---

### 2.4 Bluetooth Module Wiring (for Android Radio → Arduino)

Use an HC-05 or HC-06 Bluetooth module connected to the Arduino via hardware serial (or SoftwareSerial on spare pins):

```
HC-05 / HC-06               Arduino Nano
─────────────               ────────────
VCC  ─────────────────────► 5V
GND  ─────────────────────► GND
TXD  ─────────────────────► Pin D10 (SoftwareSerial RX)
RXD  ─────────────────────► Pin D11 (SoftwareSerial TX)
                             (use 1kΩ + 2kΩ voltage divider on RXD line
                              to step Arduino 5V TX down to 3.3V for HC-05 RX)
```

The Android radio sends track metadata via **Bluetooth AVRCP profile** (Audio/Video Remote Control Profile). Many Chinese Android head units broadcast artist/track name over AVRCP, which the HC-05 can receive and forward as a serial string.

---

### 2.5 Raspberry Pi to Arduino Connection

Connect the Raspberry Pi to the Arduino via **USB** (the Arduino appears as `/dev/ttyUSB0` or `/dev/ttyACM0` on the Pi). The Pi runs the Navit D-Bus listener (see Section 4) and sends nav instructions as simple newline-delimited text or JSON over the USB serial connection.

```
Raspberry Pi USB ──────────► Arduino USB port
(e.g. /dev/ttyACM0)          (appears as Serial on Arduino side)
```

Baud rate: **115200** recommended.

---

## 3. The 3LB Protocol

### 3.1 What It Is

The 3LB (Three-Line Bus) is Volkswagen's proprietary serial protocol for sending text and graphics to the FIS/DIS monochrome LCD in the instrument cluster. It predates CAN bus integration and operates at logic level over three wires (ENA, DATA, CLK).

From the cluster's perspective, the Arduino pretends to be the OEM navigation system or radio.

### 3.2 Open Source Libraries (Use One of These)

| Library | Platform | URL | Notes |
|---------|----------|-----|-------|
| **TLBFISLib** | Arduino / ESP32 | https://github.com/domnulvlad/TLBFISLib | Most complete, actively maintained, supports full and upper FIS modes |
| **VAGFISWriter** | Arduino | https://github.com/tomaskovacik/VAGFISWriter | Original reverse-engineered implementation, works well for Passat B6 |
| **FISCuntrol** | Arduino / ESP32 | https://github.com/adamforbes92/FISCuntrol | Ready-to-use implementation, author uses it in their own car |
| **FISBlocks** | Arduino | https://github.com/ibanezgomez/FISBlocks | Combines KWP1281 OBD reading AND 3LB FIS writing in one device |

**Recommended:** Start with **TLBFISLib** or **FISCuntrol** — both have clear documentation and confirmed working reports on Passat B6 clusters.

### 3.3 Display Regions

The FIS screen is split into two regions:

| Region | Size | Typical use |
|--------|------|-------------|
| Upper line | 2 lines × ~20 chars | Radio station name, navigation street name |
| Lower region | Full screen mode | OBD data, trip computer, custom text |

Navigation text (turn instruction, street name) is best sent to the **upper region**, which is what the OEM navigation system uses.

### 3.4 Cluster Coding Prerequisite

Before the cluster will accept 3LB data for the navigation slot, it must be coded to expect a navigation source. This requires **VCDS** (Ross-Tech) or **OBDeleven**:

- Module **17 — Instruments**
- Enable "Navigation present" in the coding byte
- This activates the Navigation menu entry in the FIS

Without this coding, the upper display region will remain blank even if the Arduino sends correct data.

---

## 4. Navit D-Bus Interface

This section summarises the interface defined in the full documentation at:
`docs/development/navit_dbus_external_dashboards.rst`
(branch: `feature/navit-dash`, repo: `https://github.com/Supermagnum/navit`)

### 4.1 Prerequisites

- Linux with D-Bus session bus
- Navit installed with D-Bus binding enabled in `navit.xml`:

```xml
<plugin path="$NAVIT_LIBDIR/*/${NAVIT_LIBPREFIX}libbinding_dbus.so" active="yes"/>
```

- Python packages: `python3-dbus`, `python3-gi`

### 4.2 D-Bus Service Details

| Item | Value |
|------|-------|
| Service name | `org.navit_project.navit` |
| Bus type | Session bus |
| Main object path | `/org/navit_project/navit/default_navit` |
| Route object path | `/org/navit_project/navit/default_navit/default_route` |
| Vehicle object path | `/org/navit_project/navit/default_navit/default_vehicle` |

### 4.3 Subscription Model

1. Connect to session bus
2. Call `callback_attr_new()` on the main object → returns a callback path
3. Call `add_attr(callback_path, attr_name)` for each attribute to watch
4. Listen passively for `callback_attr_update(attr_name, value)` signals

No polling. All updates are event-driven signals.

### 4.4 Key Attributes to Subscribe To

| Attribute | Source Object | Meaning | Example Value |
|-----------|--------------|---------|---------------|
| `navigation_next_turn` | Route | Upcoming maneuver type | `turn_right` |
| `navigation_distance_turn` | Route | Metres to next turn | `350` |
| `navigation_street_name` | Route | Next street name | `"Main Street"` |
| `navigation_street_name_systematic` | Route | Route number | `"E45"` |
| `navigation_status` | Route | Routing state | `routing` |
| `position_coord_geo` | Vehicle | GPS lat/lon | `(59.9, 10.7)` |
| `position_speed` | Vehicle | Current speed | `50.0` |
| `position_direction` | Vehicle | Heading in degrees | `90` |
| `eta` | Route | Estimated arrival time | Unix timestamp |
| `destination_length` | Route | Remaining distance (m) | `12000` |

### 4.5 Navigation Status Values

| Value | Meaning |
|-------|---------|
| `no_route` | No active route |
| `no_destination` | No destination set |
| `position_wait` | Waiting for GPS |
| `calculating` | Initial route calculation |
| `recalculating` | Route being recalculated |
| `routing` | Active, guiding |

### 4.6 Maneuver Codes → FIS Text

Since the FIS is a small monochrome display, maneuver codes must be converted to short text strings (or abbreviations) before being sent via 3LB. Suggested mappings:

| `navigation_next_turn` value | FIS text suggestion |
|------------------------------|---------------------|
| `straight` | `STRAIGHT` |
| `turn_left` | `TURN LEFT` |
| `turn_right` | `TURN RIGHT` |
| `turn_slight_left` | `SLIGHT LEFT` |
| `turn_slight_right` | `SLIGHT RIGHT` |
| `turn_sharp_left` | `SHARP LEFT` |
| `turn_sharp_right` | `SHARP RIGHT` |
| `roundabout_r1` … `r6` | `ROUNDABT EXIT n` |
| `u_turn` | `U-TURN` |
| `keep_left` | `KEEP LEFT` |
| `keep_right` | `KEEP RIGHT` |
| `destination` | `DESTINATION` |
| `recalculating` | `RECALCULATING` |

### 4.7 Python D-Bus Listener (Minimal)

This runs on the Raspberry Pi and forwards nav data to the Arduino over USB serial:

```python
#!/usr/bin/env python3
"""
Navit D-Bus listener → Arduino serial bridge.
Forwards navigation updates to Arduino via /dev/ttyACM0 for FIS display.
"""

import sys
import serial
import dbus
import dbus.mainloop.glib
from gi.repository import GLib

SERIAL_PORT = "/dev/ttyACM0"
BAUD = 115200

WATCHED = [
    "navigation_next_turn",
    "navigation_distance_turn",
    "navigation_street_name",
    "navigation_status",
]

MANEUVER_TEXT = {
    "straight":          "STRAIGHT",
    "turn_left":         "TURN LEFT",
    "turn_right":        "TURN RIGHT",
    "turn_slight_left":  "SLIGHT LEFT",
    "turn_slight_right": "SLIGHT RIGHT",
    "turn_sharp_left":   "SHARP LEFT",
    "turn_sharp_right":  "SHARP RIGHT",
    "u_turn":            "U-TURN",
    "keep_left":         "KEEP LEFT",
    "keep_right":        "KEEP RIGHT",
    "destination":       "DESTINATION",
    "recalculating":     "RECALCULATING",
}

def maneuver_text(code):
    if code.startswith("roundabout_r"):
        n = code.replace("roundabout_r", "")
        return f"ROUNDABT EXIT {n}"
    return MANEUVER_TEXT.get(code, code.upper()[:16])

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
        if attr_name == "navigation_next_turn":
            line1 = maneuver_text(str(value))
        elif attr_name == "navigation_distance_turn":
            m = int(value)
            line1 = f"{m}m" if m < 1000 else f"{m//1000}.{(m%1000)//100}km"
        elif attr_name == "navigation_street_name":
            line1 = str(value)[:16]
        elif attr_name == "navigation_status":
            line1 = str(value).upper()[:16]
        else:
            return
        # Send to Arduino as: NAV:<text>\n
        msg = f"NAV:{line1}\n"
        self.ser.write(msg.encode())

    def run(self):
        GLib.MainLoop().run()

if __name__ == "__main__":
    NavitBridge().run()
```

---

## 5. Arduino Sketch (Outline)

The Arduino receives lines from the Pi over USB serial and from the Bluetooth module (track names), then writes to the FIS using TLBFISLib:

```cpp
#include <TLBFISLib.h>       // https://github.com/domnulvlad/TLBFISLib
#include <SoftwareSerial.h>

// 3LB pins to cluster T32a
#define FIS_ENA   2
#define FIS_DATA  6
#define FIS_CLK   7

// Bluetooth module on SoftwareSerial
SoftwareSerial btSerial(10, 11); // RX, TX

TLBFISLib fis(FIS_ENA, FIS_DATA, FIS_CLK);

String navLine   = "NAVIT READY";
String mediaLine = "";

void setup() {
    Serial.begin(115200);   // USB serial from Raspberry Pi
    btSerial.begin(9600);   // Bluetooth module
    fis.begin();
}

void loop() {
    // Read nav data from Pi
    if (Serial.available()) {
        String msg = Serial.readStringUntil('\n');
        msg.trim();
        if (msg.startsWith("NAV:")) {
            navLine = msg.substring(4);
        }
    }

    // Read media/call data from Bluetooth
    if (btSerial.available()) {
        String bt = btSerial.readStringUntil('\n');
        bt.trim();
        if (bt.length() > 0) {
            mediaLine = bt.substring(0, 16);
        }
    }

    // Write to FIS upper display
    // Line 1: navigation instruction
    // Line 2: track name or empty
    fis.setLine(0, navLine.c_str());
    fis.setLine(1, mediaLine.c_str());
    fis.update();

    delay(200);
}
```

> Exact TLBFISLib API calls (`setLine`, `update`) — check the library's own examples as the API may differ slightly by version.

---

## 6. Software Stack Summary

| Layer | Technology | Where it runs |
|-------|-----------|---------------|
| Navigation engine | Navit | Raspberry Pi |
| Nav data interface | Navit D-Bus binding (`libbinding_dbus.so`) | Raspberry Pi |
| D-Bus listener + serial bridge | Python 3 (`dbus`, `pyserial`) | Raspberry Pi |
| FIS protocol encoder | TLBFISLib or VAGFISWriter (Arduino C++) | Arduino Nano |
| Bluetooth media receiver | HC-05 + SoftwareSerial | Arduino Nano |
| FIS display driver | 3LB hardware (ENA/DATA/CLK) | Arduino → Cluster |

---

## 7. Key References

| Resource | URL |
|----------|-----|
| Navit D-Bus external dashboard spec | https://github.com/Supermagnum/navit/blob/feature/navit-dash/docs/development/navit_dbus_external_dashboards.rst |
| TLBFISLib (Arduino 3LB library) | https://github.com/domnulvlad/TLBFISLib |
| VAGFISWriter (original 3LB implementation) | https://github.com/tomaskovacik/VAGFISWriter |
| FISCuntrol (complete Arduino project) | https://github.com/adamforbes92/FISCuntrol |
| FISBlocks (3LB + KWP1281 OBD combined) | https://github.com/ibanezgomez/FISBlocks |
| Passat B6 CAN traces (PQ46 platform) | https://github.com/rusefi/rusefi_documentation/tree/master/OEM-Docs/VAG/2006-Passat-B6 |
| Hackaday: Navit on VW OEM head unit | https://hackaday.io/project/8128-navit-on-a-vw-oem-navigation-system |
| Hackaday: VW CAN bus gaming cluster | https://hackaday.io/project/6288-volkswagen-can-bus-gaming |
| SVG maneuver icons for dashboards | https://github.com/Supermagnum/navit/tree/feature/navit-dash/docs/development/svg-examples |

---

*Platform: VW Passat B6 (3C), 2005–2010, PQ46 platform.
Cluster part numbers: 3C0 920 8xx series.*
