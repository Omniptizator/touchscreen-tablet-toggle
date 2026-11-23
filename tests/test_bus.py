import os
from pathlib import Path
import selectors
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
NAME = "io.github.TouchscreenTabletToggle"


@unittest.skipUnless(os.getenv("DBUS_SESSION_BUS_ADDRESS"), "needs dbus-run-session")
class BusTests(unittest.TestCase):
    def test_denied_allowed_persist_and_reconnect(self):
        env = dict(os.environ, DBUS_SYSTEM_BUS_ADDRESS=os.environ["DBUS_SESSION_BUS_ADDRESS"])
        with tempfile.TemporaryDirectory() as tmp:
            state = Path(tmp) / "enabled"
            for restarting in (False, True):
                with subprocess.Popen([sys.executable, str(HERE / "mock_service.py"), tmp],
                                      stdout=subprocess.PIPE, text=True, env=env) as proc:
                    try:
                        with selectors.DefaultSelector() as sel:
                            sel.register(proc.stdout, selectors.EVENT_READ)
                            self.assertTrue(sel.select(5), "service startup timed out")
                            self.assertEqual(proc.stdout.readline().strip(), "ready")
                        cmd = ["gdbus", "call", "--session", "--dest", NAME,
                               "--object-path", "/io/github/TouchscreenTabletToggle", "--method"]
                        if restarting:
                            out = subprocess.check_output(cmd + [
                                "org.freedesktop.DBus.Properties.Get", NAME, "Enabled"])
                            self.assertIn(b"true", out)
                            continue
                        result = subprocess.run(cmd + [NAME + ".SetEnabled", "true"],
                                                capture_output=True)
                        self.assertIn(b"AccessDenied", result.stderr)
                        self.assertFalse(state.exists())
                        (Path(tmp) / "allowed").touch()
                        if shutil.which("gjs"):
                            subprocess.run(["gjs", "-m", str(HERE / "client.js")],
                                           check=True, timeout=10, env=env)
                        else:
                            subprocess.run(cmd + [NAME + ".SetEnabled", "true"], check=True)
                        self.assertEqual(state.read_text(), "1\n")
                    finally:
                        proc.terminate()
