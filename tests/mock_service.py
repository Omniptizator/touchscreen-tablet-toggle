"""Private-bus fixture. No input devices, system bus or real polkit."""
from pathlib import Path
import signal
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "service"))
from toggle import Gio, GLib, Service, NAME, ACTION


class Hardware:
    enabled = blocked = False
    reason = ""
    mode = 0

    def __init__(self, notify):
        self.notify = notify

    def apply(self):
        self.blocked = self.enabled and self.mode == 0
        self.notify()

    refresh = apply


def main():
    bus = Gio.bus_get_sync(Gio.BusType.SESSION)
    fixture = Path(sys.argv[1])
    info = Gio.DBusNodeInfo.new_for_xml("""<node>
    <interface name="org.freedesktop.PolicyKit1.Authority">
      <method name="CheckAuthorization">
        <arg type="(sa{sv})" direction="in"/><arg type="s" direction="in"/>
        <arg type="a{ss}" direction="in"/><arg type="u" direction="in"/>
        <arg type="s" direction="in"/><arg type="(bba{ss})" direction="out"/>
      </method></interface></node>""")

    def check(_bus, _sender, _path, _iface, _method, args, call):
        subject, action, _, flags, _ = args.unpack()
        allowed = (action == ACTION and subject[0] == "system-bus-name"
                   and subject[1]["name"].startswith(":") and flags == 0
                   and (fixture / "allowed").exists())
        call.return_value(GLib.Variant("((bba{ss}))", ((allowed, False, {}),)))

    bus.register_object("/org/freedesktop/PolicyKit1/Authority",
                        info.interfaces[0], check, None, None)
    Gio.bus_own_name_on_connection(bus, "org.freedesktop.PolicyKit1",
                                   Gio.BusNameOwnerFlags.NONE, None, None)
    service = Service(bus, fixture / "enabled", Hardware)
    def fold():
        service.hw.mode = 1
        service.hw.apply()
        return True

    from gi.repository import GLibUnix
    GLibUnix.signal_add(GLib.PRIORITY_DEFAULT, signal.SIGUSR1, fold)
    Gio.bus_own_name_on_connection(bus, NAME, Gio.BusNameOwnerFlags.NONE,
                                   lambda *_: print("ready", flush=True), None)
    GLib.MainLoop().run()


if __name__ == "__main__":
    main()
