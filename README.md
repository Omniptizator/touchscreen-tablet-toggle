# Tablet-only Touchscreen

A GNOME Quick Setting for Ubuntu 26.04 (Shell 50) and Fedora Workstation 45
(Shell 51) convertibles. Enable
**Tablet-only touchscreen** to ignore finger taps in laptop posture and
automatically accept them again in tablet posture. The policy defaults to off.

| Policy | Intel tablet switch | Finger input |
| --- | --- | --- |
| Off | Any | Normal |
| On | `0` (laptop) | Blocked |
| On | `1` (tablet) | Normal |
| On | Missing, unreadable or ambiguous hardware | Normal |

The policy is machine-wide. It survives logout and reboot and applies at
the login screen. Disabling the **extension** hides its UI; turn off the
**Quick Setting** to stop blocking touch. The switch threshold is determined
by firmware, not by a configured hinge angle.

## Hardware and release status

This release targets exactly one `Intel Virtual Switches` device advertising
`SW_TABLET_MODE` and exactly one udev-tagged multitouch finger device on an
internal I²C or SPI bus. No HP or ELAN identifiers are hard-coded. USB panels
(even internal ones), combined pen/finger devices, and ambiguous configurations
are intentionally unsupported in this release. The separate pen, touchpad and
keyboard are not grabbed.

The Intel switch and ELAN finger device were identified on an HP Spectre
x360 15-bl012dx, where the user confirmed operation on Ubuntu. Physical
Fedora hardware validation is pending. No PPA or Copr has been published yet.

## Install and enable

On Ubuntu 26.04, with the locally built or downloaded package:

```sh
sudo apt install ./touchscreen-tablet-toggle_0.1.1_all.deb
```

On Fedora Workstation 45, from the directory containing the binary RPM:

```sh
sudo dnf install ./touchscreen-tablet-toggle-0.1.1-1.fc45.noarch.rpm
sudo systemctl enable --now touchscreen-tablet-toggle.service
```

The Fedora package respects systemd presets. The explicit one-time service
enablement makes the backend available at boot; it does not enable the policy.

Log out and back in so GNOME discovers the system extension. Enable
**Tablet-only Touchscreen** in the Extensions app, or run:

```sh
gnome-extensions enable tablet-only-touchscreen@touchscreen-tablet-toggle
```

Open Quick Settings and turn on **Tablet-only touchscreen**. Its subtitle
shows whether finger input is blocked, available, or unsupported. Any active
local user can change this preference; remote and inactive sessions cannot.

## How it works

A systemd service reads the switch's initial state via libevdev, listens for
changes, and rescans after input hotplug and resume. In laptop mode it holds
an exclusive read grab (`EVIOCGRAB`) on the finger input event node. Closing
the descriptor releases the grab, including on process death. It does not
change the kernel's `inhibited` flag or unload any drivers. No touchscreen
events are logged or stored. The only persisted value is the on/off preference.

The extension reads status from `io.github.TouchscreenTabletToggle` on the
system bus. Its only mutation method is `SetEnabled(b)`, authorized by polkit.
The service needs read access to input devices, not membership of human users
in the `input` group.

## Diagnostics and removal

```sh
# Read-only discovery; does not open or grab event nodes
/usr/bin/python3 /usr/lib/touchscreen-tablet-toggle/hardware.py
systemctl status touchscreen-tablet-toggle
journalctl -b -u touchscreen-tablet-toggle
gdbus introspect --system --dest io.github.TouchscreenTabletToggle \
  --object-path /io/github/TouchscreenTabletToggle
```

If the extension is unavailable, an active local user can turn off the policy:

```sh
gdbus call --system --dest io.github.TouchscreenTabletToggle \
  --object-path /io/github/TouchscreenTabletToggle \
  --method io.github.TouchscreenTabletToggle.SetEnabled false
```

Stopping the service also releases any grab immediately. To remove it and
its stored preference:

```sh
gnome-extensions disable tablet-only-touchscreen@touchscreen-tablet-toggle
sudo apt purge touchscreen-tablet-toggle
```

On Fedora use `sudo dnf remove touchscreen-tablet-toggle` instead of `apt
purge`. RPM removal leaves the preference in `/var/lib/touchscreen-tablet-toggle/`.

## Development and packaging

Build on Ubuntu 26.04 (or inside its container). From the source tree:

```sh
sudo apt install build-essential dpkg-dev debhelper python3-gi python3-libevdev \
  python3-pyudev dbus-daemon libglib2.0-bin gjs
dbus-run-session -- /usr/bin/python3 -m unittest discover -s tests -v
dpkg-buildpackage -us -uc
```

The unsigned native source package and architecture-independent `.deb` are
written one directory above the tree. The private-bus integration test uses
mock hardware and a mock authorization authority; with `gjs` available it
exercises the same D-Bus client as the Quick Setting.

For Fedora, use `bash packaging/fedora/build.sh` in the Fedora 45 build
environment defined in `.github/workflows/fedora.yml`. The binary and source
RPMs and checksums are written to `dist/fedora/`.

See [RELEASING.md](RELEASING.md) for hardware acceptance and publication.
License: GPL-3.0-or-later.
