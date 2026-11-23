# Release checklist

## 0.2.0 native release

The service and test fixtures are C. The packaged GNOME extension and its
D-Bus contract are unchanged. Rollback binaries live in `dist/ubuntu/` and
`dist/fedora/`; the pre-migration source is tagged `python-baseline`.

## Local acceptance before tagging

For the Ubuntu upgrade, verification commands, and rollback, see
[Spectre cutover](docs/native-cutover.md).

1. Install the `.deb` and enable the extension. The default is off. Enable
   the policy in laptop mode: finger taps should stop immediately; pen and
   touchpad should still work as they ordinarily do in this posture.
2. Fold and unfold. Verify the subtitle and finger input in both positions.
   Start in tablet mode and toggle the policy off and on: touch must work
   without first moving the hinge. Switch off in laptop posture: touch returns.
3. Leave the policy on, suspend, change posture, and resume. Repeat across
   logout and reboot. Confirm an attached USB touch device is unaffected.
4. Stop the service while touch is blocked. Verify touch returns; restart and
   check the policy reapplies. Plug and unplug a mouse in laptop posture;
   finger touch should stay blocked.
5. Verify `readlink /proc/$(systemctl show -p MainPID --value touchscreen-tablet-toggle)/exe`
   (with sudo if needed) and `busctl --system status io.github.TouchscreenTabletToggle`
   identify the installed native daemon. Check `/usr/lib/touchscreen-tablet-toggle/`
   for leftover bytecode. Compare with [the resource profile](docs/resource-profile.md).

## GitHub

Push the tested revision and its version tag. The native packages workflow
rebuilds both packages, exercises the installed Quick Setting, and publishes
the `.deb`, RPMs and SHA-256 checksums on tagged builds after both jobs pass.

## Launchpad PPA

Create the PPA and configure a Launchpad-registered signing key. Use the
same source as the tested tag with a new changelog entry and a PPA-specific
version, e.g. `0.2.0+ppa1` for `resolute`. Build a **signed source upload**:

```sh
dpkg-buildpackage -S -sa -k<KEY_FINGERPRINT>
dput ppa:<OWNER>/<PPA> ../touchscreen-tablet-toggle_0.2.0+ppa1_source.changes
```

After Launchpad publishes the binary, verify on a fresh Ubuntu 26.04 system:

```sh
sudo add-apt-repository ppa:<OWNER>/<PPA>
sudo apt install touchscreen-tablet-toggle
```

Publish the actual PPA name only after this succeeds. Keep binary and source
versions immutable; increment the changelog version for subsequent uploads.

## Fedora Copr

Build from the tested source with `bash packaging/fedora/build.sh`. Run the
container checks in `.github/workflows/packages.yml`. Before claiming physical
Fedora support, test on Workstation 45 with SELinux enforcing and an Intel
tablet-mode switch. Verify finger/pen separation across folding and resume.

After setting up the Copr project and credentials, enable its
`fedora-45-x86_64` chroot and submit the source RPM:

```sh
copr-cli build OWNER/PROJECT dist/native/fedora/touchscreen-tablet-toggle-0.2.0-1.fc45.src.rpm
```

Verify the resulting **Copr-signed build** from a clean Fedora system:

```sh
sudo dnf copr enable OWNER/PROJECT
sudo dnf install touchscreen-tablet-toggle
sudo systemctl enable --now touchscreen-tablet-toggle.service
```

Then enable the GNOME extension per user as in the README. Publish the actual
repository name after verification. A local release-candidate RPM is unsigned.
