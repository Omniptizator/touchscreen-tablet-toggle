"""Exercise actual GLib FD watches without touching the machine's input."""
import os
import sys
import unittest
from contextlib import ExitStack
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "service"))
import hardware as h


class TransitionTests(unittest.TestCase):
    def test_initial_state_events_resume_and_ambiguous_hotplug(self):
        with ExitStack() as stack:
            def pipe():
                r, w = os.pipe()
                stack.callback(os.close, w)
                return stack.enter_context(os.fdopen(r, "rb", buffering=0)), w

            sensor, sensor_writer = pipe()
            udev, udev_writer = pipe()
            monitor = Mock()
            monitor.fileno.return_value = udev.fileno()
            monitor.poll.side_effect = lambda timeout: udev.read(1) or None
            os.set_blocking(udev.fileno(), False)
            stack.enter_context(patch.object(h.pyudev.Monitor, "from_netlink",
                                             return_value=monitor))
            devices = stack.enter_context(patch.object(h, "discover",
                                                       return_value=(["sw"], ["touch"])))
            switch = Mock(file=sensor)
            switch.dev.name = h.SWITCH
            switch.dev.value = {h.MODE: 0}

            def events():
                switch.dev.value[h.MODE] = int(sensor.read(1))
                yield None

            switch.dev.events = events
            stack.enter_context(patch.object(h, "Input", return_value=switch))
            notify = Mock()
            hw = h.Hardware(notify)
            stack.callback(h.GLib.source_remove, hw.monitor_watch)
            stack.callback(hw.reset)
            touch = Mock()
            touch.dev.id = {"bustype": 0x18}
            touch.dev.evbits = {h.libevdev.EV_ABS: [h.libevdev.EV_ABS.ABS_MT_POSITION_X]}
            hw.policy.open_input = Mock(return_value=touch)
            hw.enabled = True
            hw.refresh()
            self.assertTrue(hw.blocked)
            self.assertEqual(hw.reason, "")

            ctx = h.GLib.MainContext.default()
            for mode in [1, 0]:
                os.write(sensor_writer, str(mode).encode())
                ctx.iteration(False)
                self.assertEqual(hw.mode, mode)
                self.assertEqual(hw.blocked, mode == 0)

            switch.dev.value[h.MODE] = 1  # posture changed during sleep
            hw.refresh()
            self.assertFalse(hw.blocked)
            self.assertEqual(hw.mode, 1)
            devices.return_value = (["sw"], ["touch", "second-panel"])
            os.write(udev_writer, b"+")
            while ctx.pending():
                ctx.iteration(False)
            self.assertEqual(hw.mode, -1)
            self.assertFalse(hw.blocked)
            self.assertIn("No unique", hw.reason)
