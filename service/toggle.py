"""Machine-wide policy service. No bus method accepts device or file paths."""
import os
import signal
from pathlib import Path

from gi.repository import Gio, GLib, GLibUnix

from hardware import Hardware

NAME = "io.github.TouchscreenTabletToggle"
PATH = "/io/github/TouchscreenTabletToggle"
ACTION = "io.github.touchscreen-tablet-toggle.configure"
XML = f"""<node><interface name="{NAME}">
  <method name="SetEnabled"><arg name="enabled" type="b" direction="in"/></method>
  <property name="Enabled" type="b" access="read"/>
  <property name="TabletMode" type="i" access="read"/>
  <property name="Blocked" type="b" access="read"/>
  <property name="Problem" type="s" access="read"/>
</interface></node>"""
V = GLib.Variant


def authorize(bus, sender, reply):
    def done(conn, result):
        try:
            allowed = conn.call_finish(result).unpack()[0][0]
        except GLib.Error:
            allowed = False
        reply(allowed)

    bus.call("org.freedesktop.PolicyKit1", "/org/freedesktop/PolicyKit1/Authority",
             "org.freedesktop.PolicyKit1.Authority", "CheckAuthorization",
             V("((sa{sv})sa{ss}us)", (("system-bus-name", {"name": V("s", sender)}),
                                       ACTION, {}, 0, "")),
             GLib.VariantType.new("((bba{ss}))"), Gio.DBusCallFlags.NONE,
             5000, None, done)


class Service:
    def __init__(self, bus, state, hardware=Hardware, check=authorize):
        self.bus, self.state, self.check = bus, state, check
        self.hw = hardware(self.changed)
        self.hw.enabled = state.exists() and state.read_text().strip() == "1"
        self.info = Gio.DBusNodeInfo.new_for_xml(XML).interfaces[0]
        bus.register_object(PATH, self.info, self.method, self.get, None)
        self.hw.refresh()

    def props(self):
        return {"Enabled": V("b", self.hw.enabled), "TabletMode": V("i", self.hw.mode),
                "Blocked": V("b", self.hw.blocked), "Problem": V("s", self.hw.reason)}

    def changed(self):
        self.bus.emit_signal(None, PATH, "org.freedesktop.DBus.Properties",
                             "PropertiesChanged", V("(sa{sv}as)", (NAME, self.props(), [])))

    def get(self, bus, sender, path, iface, prop):
        return self.props()[prop]

    def method(self, bus, sender, path, iface, method, params, invocation):
        enabled, = params.unpack()

        def complete(allowed):
            if not allowed:
                invocation.return_dbus_error("org.freedesktop.DBus.Error.AccessDenied",
                                             "An active local session is required")
                return
            try:
                tmp = self.state.with_suffix(".tmp")
                tmp.write_text("1\n" if enabled else "0\n")
                tmp.replace(self.state)
                self.hw.enabled = enabled
                self.hw.apply()
            except OSError as exc:
                invocation.return_dbus_error(NAME + ".Error", str(exc))
                return
            invocation.return_value(None)

        self.check(bus, sender, complete)


def main():
    loop = GLib.MainLoop()
    bus = Gio.bus_get_sync(Gio.BusType.SYSTEM, None)
    service = Service(bus, Path(os.environ["STATE_DIRECTORY"]) / "enabled")
    owner = Gio.bus_own_name_on_connection(bus, NAME, Gio.BusNameOwnerFlags.NONE,
                                           None, lambda *_: loop.quit())
    def sleep_changed(_bus, _sender, _path, _iface, _signal, args):
        if not args.unpack()[0]:
            service.hw.refresh()

    bus.signal_subscribe("org.freedesktop.login1", "org.freedesktop.login1.Manager",
                         "PrepareForSleep", "/org/freedesktop/login1", None,
                         Gio.DBusSignalFlags.NONE, sleep_changed)
    GLibUnix.signal_add(GLib.PRIORITY_DEFAULT, signal.SIGTERM, loop.quit)
    try:
        loop.run()
    finally:
        service.hw.reset()
        Gio.bus_unown_name(owner)


if __name__ == "__main__":
    main()
