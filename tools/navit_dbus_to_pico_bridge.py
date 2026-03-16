#!/usr/bin/env python3
"""
Navit D-Bus listener -> Pico 2 W serial bridge with eco_mode_fuel_enabled support.

Forwards Navit D-Bus attributes to the Pico over USB CDC or Bluetooth SPP.
When the Driver Break plugin exposes eco_mode_fuel_enabled (ECU or adaptive fuel
in use) and a route has just been calculated, sends the eco icon for a short
period then the real first maneuver so the FIS never shows eco instead of routing.

Requires: Python 3, pyserial, PyGObject (gi.repository.GLib), dbus.
Configure SERIAL_PORT and optionally ECO_DISPLAY_SECONDS below.
"""

from __future__ import annotations

import dbus
import dbus.mainloop.glib
import sys
import argparse
from gi.repository import GLib

try:
    import serial
except ImportError:
    sys.exit("pyserial required: pip install pyserial")

BAUD = 115200

WATCHED = [
    "navigation_next_turn",
    "navigation_distance_turn",
    "navigation_street_name",
    "navigation_status",
    "eta",
    "destination_length",
    "position_direction",
    "position_time_iso8601",
    "position_coord_geo",
]

MANEUVER_MAP = {
    "straight": "straight",
    "turn_left": "turn_left",
    "turn_right": "turn_right",
    "turn_slight_left": "slight_left",
    "turn_slight_right": "slight_right",
    "turn_sharp_left": "sharp_left",
    "turn_sharp_right": "sharp_right",
    "u_turn": "u_turn",
    "keep_left": "keep_left",
    "keep_right": "keep_right",
    "destination": "destination",
}


def get_navit_object_and_path(bus):
    """Resolve navit instance path and return (navit_obj, navit_iface) for get_attr.
    See driver-break dbus.rst: get_attr_wi('navit', attr_iter) on root; path[1] is navit path.
    """
    root_path = "/org/navit_project/navit"
    root = bus.get_object("org.navit_project.navit", root_path)
    root_iface = dbus.Interface(root, dbus_interface="org.navit_project.navit")
    try:
        attr_iter = root_iface.attr_iter()
        result = root_iface.get_attr_wi("navit", attr_iter)
        navit_path = result[1] if (result and len(result) > 1) else "/org/navit_project/navit/default_navit"
        root_iface.attr_iter_destroy(attr_iter)
    except (dbus.DBusException, TypeError, IndexError, AttributeError):
        navit_path = "/org/navit_project/navit/default_navit"
    navit = bus.get_object("org.navit_project.navit", navit_path)
    navit_iface = dbus.Interface(navit, dbus_interface="org.navit_project.navit.navit")
    return navit, navit_iface


def get_eco_mode_fuel_enabled(navit_iface):
    """Return True if Driver Break plugin reports eco_mode_fuel_enabled (ECU or adaptive fuel)."""
    try:
        _attrname, value = navit_iface.get_attr("eco_mode_fuel_enabled")
        return bool(value)
    except (dbus.DBusException, TypeError):
        return False


class NavitBridge:
    def __init__(self, serial_port: str, eco_display_seconds: float):
        self.ser = serial.Serial(serial_port, BAUD, timeout=1)
        self.eco_display_seconds = eco_display_seconds
        self.last_turn = ""
        self.last_dist = 0
        self.showing_eco = False
        self.eco_timer_id = None
        self.navit_iface = None

        dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
        bus = dbus.SessionBus()
        obj = bus.get_object("org.navit_project.navit", "/org/navit_project/navit/default_navit")
        iface = dbus.Interface(obj, dbus_interface="org.navit_project.navit")

        try:
            _navit_obj, self.navit_iface = get_navit_object_and_path(bus)
        except (dbus.DBusException, TypeError):
            self.navit_iface = None

        cb_path = iface.callback_attr_new()
        for attr in WATCHED:
            iface.add_attr(cb_path, attr)
        bus.add_signal_receiver(
            self.on_update,
            signal_name="callback_attr_update",
            dbus_interface="org.navit_project.navit",
            path=cb_path,
        )

    def _send(self, msg: str) -> None:
        if msg:
            self.ser.write(msg.encode())

    def _send_turn(self, code: str) -> None:
        self._send(f"NAV:TURN:{code}\n")

    def _send_dist(self, metres: int) -> None:
        self._send(f"NAV:DIST:{metres}\n")

    def _eco_timer_fire(self) -> bool:
        self.showing_eco = False
        self.eco_timer_id = None
        code = MANEUVER_MAP.get(self.last_turn, self.last_turn) if self.last_turn else "straight"
        self._send_turn(code)
        self._send_dist(max(0, self.last_dist))
        return False

    def _start_eco_brief_display(self) -> None:
        if self.showing_eco or self.eco_timer_id is not None:
            return
        if self.navit_iface is None or not get_eco_mode_fuel_enabled(self.navit_iface):
            return
        self.showing_eco = True
        self._send_turn("eco_mode")
        ms = int(self.eco_display_seconds * 1000)
        self.eco_timer_id = GLib.timeout_add(ms, self._eco_timer_fire)

    def on_update(self, attr_name: str, value) -> None:
        msg = None
        if attr_name == "navigation_next_turn":
            self.last_turn = str(value)
            if not self.showing_eco:
                code = MANEUVER_MAP.get(self.last_turn, self.last_turn)
                msg = f"NAV:TURN:{code}\n"
        elif attr_name == "navigation_distance_turn":
            self.last_dist = int(value)
            if not self.showing_eco:
                msg = f"NAV:DIST:{self.last_dist}\n"
        elif attr_name == "navigation_street_name":
            msg = f"NAV:STREET:{str(value)[:20]}\n"
        elif attr_name == "navigation_status":
            status = str(value)
            msg = f"NAV:STATUS:{status}\n"
            if status == "routing":
                self._start_eco_brief_display()
        elif attr_name == "eta":
            msg = f"NAV:ETA:{int(value)}\n"
        elif attr_name == "destination_length":
            msg = f"NAV:REMAIN:{int(value)}\n"
        elif attr_name == "position_direction":
            v = int(value)
            if 0 <= v <= 360:
                msg = f"NAV:HEAD:{v}\n"
        elif attr_name == "position_time_iso8601":
            msg = f"NAV:TIME:{str(value)}\n"
        elif attr_name == "position_coord_geo":
            try:
                lat, lon = float(value[1]), float(value[0])
                msg = f"NAV:POS:{lat:.4f},{lon:.4f}\n"
            except (IndexError, TypeError, ValueError):
                pass
        if msg:
            self._send(msg)

    def run(self) -> None:
        GLib.MainLoop().run()


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Navit D-Bus to Pico serial bridge with eco_mode_fuel_enabled support."
    )
    ap.add_argument(
        "--port",
        default="/dev/ttyACM0",
        help="Serial port (USB CDC or Bluetooth SPP, e.g. /dev/rfcomm0)",
    )
    ap.add_argument(
        "--eco-seconds",
        type=float,
        default=2.5,
        metavar="SEC",
        help="Seconds to show eco icon after route is ready when eco_mode_fuel_enabled (default: 2.5)",
    )
    args = ap.parse_args()
    bridge = NavitBridge(serial_port=args.port, eco_display_seconds=args.eco_seconds)
    bridge.run()


if __name__ == "__main__":
    main()
