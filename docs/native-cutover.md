# Spectre cutover

Use the locally verified candidate in `dist/native/ubuntu/`. This procedure
leaves the existing machine-wide preference and per-user extension setting
in place. Run it from the repository on the Ubuntu laptop.

```sh
(cd dist/native/ubuntu && sha256sum -c binary.sha256)
sudo apt install ./dist/native/ubuntu/touchscreen-tablet-toggle_0.2.0_amd64.deb
sudo ./verify-installed
```

With the Quick Setting enabled:

1. In laptop posture, verify finger taps are ignored. Check the pen and touchpad.
2. Plug in and unplug a mouse; the subtitle and finger blocking should stay
   in laptop mode.
3. Fold the machine and tap the screen. Finger touch should work before and
   after disabling and re-enabling the tile while folded.
4. Suspend, change posture, and resume. Verify the subtitle and finger input
   agree without a second hinge movement. Return the tile to its desired state.

Rollback to the package previously installed on this Spectre:

```sh
(cd dist && sha256sum -c SHA256SUMS)
sudo apt install --allow-downgrades ./dist/touchscreen-tablet-toggle_0.1.0_all.deb
```

## Retirement inventory

On the HP Spectre x360 the user verified the native service PID, normal touch
when folded and blocked touch in laptop mode. The six old source files have
been removed from the working tree. Rollback source is `python-baseline`;
keep the archived packages, checksums, and measurements.

At migration start there was no repository venv or bytecode cache. The former
`/tmp/opencode/ttt-deps` tree and `ttt-profile-lab.py` were already absent.
Ubuntu had `python3-libevdev` and `python3-pyudev` automatically installed;
the old package was their only installed reverse dependency. Recheck after
cutover. If no other software needs them, simulate **targeted** removal with
`apt-get -s remove python3-libevdev python3-pyudev` before removing those
two packages. Leave shared `python3-gi` and the system interpreter installed.

`verify-installed` checks the executable, D-Bus owner, and installed remnants.
