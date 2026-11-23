"""Input discovery and the event-driven tablet-only touch policy."""
import os

import libevdev
from libevdev.device import DeviceGrabError
import pyudev
from gi.repository import GLib, GLibUnix


SWITCH = "Intel Virtual Switches"
MODE = libevdev.EV_SW.SW_TABLET_MODE


def select(devices):
    """Return unique candidates only. Device paths/numbers are never settings."""
    switches, screens = [], []
    for dev in devices:
        parent = dev.parent
        if parent is None or not dev.device_node:
            continue
        attr = parent.attributes
        if (attr.get("name") == SWITCH.encode()
                and int(attr.get("capabilities/sw", b"0").split()[-1], 16) & 2):
            switches.append(dev.device_node)
        # v1 deliberately skips USB: even a built-in USB panel cannot be
        # reliably distinguished from a portable external monitor by bus alone.
        if (dev.get("ID_INPUT_TOUCHSCREEN") == "1"
                and attr.get("id/bustype") in (b"0018", b"001c")  # I2C, SPI
                and not any(dev.get(k) == "1" for k in (
                    "ID_INPUT_TABLET", "ID_INPUT_TOUCHPAD", "ID_INPUT_KEYBOARD",
                    "ID_INPUT_TABLET_PAD", "ID_INPUT_MOUSE"))):
            screens.append(dev.device_node)
    return switches, screens


def discover(context):
    return select(context.list_devices(subsystem="input", sys_name="event*"))


class Input:
    def __init__(self, path):
        self.file = open(path, "rb", buffering=0)
        try:
            os.set_blocking(self.file.fileno(), False)
            self.dev = libevdev.Device(self.file)
        except Exception:
            self.file.close()
            raise

    def close(self):
        self.file.close()  # also releases EVIOCGRAB on exit/crash


class Policy:
    def __init__(self, open_input=Input):
        self.open_input = open_input
        self.held = None

    def release(self):
        if self.held:
            self.held.close()
            self.held = None

    def apply(self, enabled, mode, target):
        if not enabled or mode != 0 or not target:
            self.release()
            return False
        if not self.held:
            handle = self.open_input(target)
            try:
                codes = handle.dev.evbits
                keys = codes.get(libevdev.EV_KEY, [])
                if (handle.dev.id["bustype"] not in (0x18, 0x1c)
                        or not handle.dev.has_property(libevdev.INPUT_PROP_DIRECT)
                        or libevdev.EV_ABS.ABS_MT_POSITION_X not in codes.get(libevdev.EV_ABS, [])
                        or any(k.name.startswith(("BTN_TOOL_PEN", "BTN_STYLUS",
                                                  "BTN_TOOL_RUBBER")) for k in keys)):
                    raise ValueError("No isolated multitouch finger device")
                try:
                    handle.dev.grab()
                except DeviceGrabError as exc:
                    raise OSError("Touchscreen is busy or cannot be grabbed") from exc
            except Exception:
                handle.close()
                raise
            self.held = handle
        return True


class Hardware:
    def __init__(self, notify):
        self.notify = notify
        self.context = pyudev.Context()
        self.policy = Policy()
        self.switch = self.target = None
        self.watch = self.pending = 0
        self.mode = -1
        self.blocked = self.enabled = False
        self.reason = ""
        self.monitor = pyudev.Monitor.from_netlink(self.context)
        self.monitor.filter_by(subsystem="input")
        self.monitor.start()
        self.monitor_watch = GLibUnix.fd_add_full(
            GLib.PRIORITY_DEFAULT, self.monitor.fileno(), GLib.IO_IN, self._hotplug)

    def _hotplug(self, *_):
        while self.monitor.poll(timeout=0) is not None:
            pass
        if not self.pending:
            self.pending = GLib.idle_add(self.refresh)
        return True

    def reset(self):
        self.policy.release()
        if self.watch:
            GLib.source_remove(self.watch)
            self.watch = 0
        if self.switch:
            self.switch.close()
        self.switch = self.target = None
        self.mode, self.blocked = -1, False

    def refresh(self):
        if self.pending:
            GLib.source_remove(self.pending)
            self.pending = 0
        self.reset()
        try:
            switches, screens = discover(self.context)
            if len(switches) != 1:
                raise ValueError("No unique Intel tablet-mode switch")
            if len(screens) != 1:
                raise ValueError("No unique internal finger touchscreen")
            self.switch = Input(switches[0])
            if self.switch.dev.name != SWITCH or self.switch.dev.value[MODE] not in (0, 1):
                raise ValueError("Unsupported tablet-mode switch")
            self.mode = self.switch.dev.value[MODE]
            self.target = screens[0]
            self.watch = GLibUnix.fd_add_full(
                GLib.PRIORITY_DEFAULT, self.switch.file.fileno(),
                GLib.IO_IN | GLib.IO_HUP | GLib.IO_ERR, self._events)
            self.apply()
        except Exception as exc:
            self.reset()
            self.reason = str(exc)
            self.notify()
        return False

    def _events(self, fd, condition):
        try:
            if condition & (GLib.IO_HUP | GLib.IO_ERR):
                raise OSError("Tablet-mode switch disconnected")
            try:
                for _ in self.switch.dev.events():
                    pass
            except libevdev.EventsDroppedException:
                for _ in self.switch.dev.sync():
                    pass
            self.mode = self.switch.dev.value[MODE]
            self.apply()
        except Exception as exc:
            self.watch = 0
            self.reset()
            self.reason = str(exc)
            self.notify()
            return False
        return True

    def apply(self):
        if self.target:
            try:
                self.blocked = self.policy.apply(self.enabled, self.mode, self.target)
                self.reason = ""
            except Exception as exc:
                self.blocked = False
                self.reason = str(exc)
        self.notify()


if __name__ == "__main__":
    switches, screens = discover(pyudev.Context())
    print("Switch candidates:", switches, "\nTouchscreen candidates:", screens)
