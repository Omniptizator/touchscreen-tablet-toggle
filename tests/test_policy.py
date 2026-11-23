import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "service"))
import hardware as h


class Device(dict):
    def __init__(self, node, name, bus=b"0018", sw=b"0", **tags):
        super().__init__(tags)
        self.device_node = node
        self.parent = SimpleNamespace(attributes={
            "name": name.encode(), "capabilities/sw": sw, "id/bustype": bus})


class PolicyTests(unittest.TestCase):
    def test_discovery_excludes_external_and_composite_devices(self):
        finger = {"ID_INPUT_TOUCHSCREEN": "1"}
        devices = [
            Device("hp", "HP WMI hotkeys", sw=b"22"),
            Device("wrong", h.SWITCH, sw=b"1"),
            Device("intel", h.SWITCH, sw=b"2"),
            Device("external", "Display", bus=b"0003", **finger),
            Device("pen", "Pen", ID_INPUT_TABLET="1", **finger),
            Device("pad", "Pad", ID_INPUT_TOUCHPAD="1", **finger),
            Device("touch", "Any vendor", **finger)]
        self.assertEqual(h.select(devices), (["intel"], ["touch"]))

    def fixture(self):
        opener = Mock()
        handle = opener.return_value
        handle.dev.id = {"bustype": 0x18}
        handle.dev.has_property.return_value = True
        handle.dev.evbits = {h.libevdev.EV_ABS: [h.libevdev.EV_ABS.ABS_MT_POSITION_X]}
        return h.Policy(opener), handle

    def test_fold_disconnect_and_disable_release_our_grab(self):
        policy, handle = self.fixture()
        for args in [(True, 1, "touch"), (True, -1, None), (False, 0, "touch")]:
            self.assertTrue(policy.apply(True, 0, "touch"))
            self.assertTrue(policy.apply(True, 0, "touch"))
            self.assertFalse(policy.apply(*args))
        self.assertEqual(handle.dev.grab.call_count, 3)
        self.assertEqual(handle.close.call_count, 3)

    def test_unknown_posture_and_ambiguous_screen_fail_open(self):
        policy, handle = self.fixture()
        for args in [(True, -1, "touch"), (True, 0, None)]:
            self.assertFalse(policy.apply(*args))
        handle.dev.grab.assert_not_called()

    def test_failed_or_unsafe_grab_closes_fd(self):
        policy, handle = self.fixture()
        handle.dev.grab.side_effect = h.DeviceGrabError()
        with self.assertRaisesRegex(OSError, "busy"):
            policy.apply(True, 0, "touch")
        handle.dev.evbits[h.libevdev.EV_KEY] = [h.libevdev.EV_KEY.BTN_TOOL_PEN]
        with self.assertRaises(ValueError):
            policy.apply(True, 0, "touch")
        handle.dev.grab.assert_called_once()
        self.assertIsNone(policy.held)
        self.assertEqual(handle.close.call_count, 2)
