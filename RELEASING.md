# Release checklist

## 0.1.1 release candidate

Adds Fedora Workstation 45 packaging and GNOME 51 compatibility metadata.
The Fedora RPM reuses the Ubuntu service, extension code and systemd unit.

## Local acceptance before tagging

1. Install the `.deb` and enable the extension. The default is off. Enable
   the policy in laptop mode: finger taps should stop immediately; pen and
   touchpad should still work as they ordinarily do in this posture.
2. Fold and unfold. Verify the subtitle and finger input in both positions.
   Start in tablet mode and toggle the policy off and on: touch must work
   without first moving the hinge. Switch off in laptop posture: touch returns.
3. Leave the policy on, suspend, change posture, and resume. Repeat across
   logout and reboot. Confirm an attached USB touch device is unaffected.
4. Stop the service while touch is blocked. Verify touch returns; restart and
   check the policy reapplies. Purge the package and verify normal finger input.

## GitHub

Set the real maintainer details in `debian/control` and `debian/changelog`
before publication; the `.invalid` address is a development placeholder.
Add the provided remote and push. Confirm the build workflow passes, tag the
tested revision (`v0.1.1`), and attach the built `.deb`, RPMs and SHA-256 checksums
to a GitHub release. Record exactly which machines passed hardware acceptance.

## Launchpad PPA

Create the PPA and configure a Launchpad-registered signing key. Use the
same source as the tested tag with a new changelog entry and a PPA-specific
version, e.g. `0.1.1+ppa1` for `resolute`. Build a **signed source upload**:

```sh
dpkg-buildpackage -S -sa -k<KEY_FINGERPRINT>
dput ppa:<OWNER>/<PPA> ../touchscreen-tablet-toggle_0.1.1+ppa1_source.changes
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
container checks in `.github/workflows/fedora.yml`. Before claiming physical
Fedora support, test on Workstation 45 with SELinux enforcing and an Intel
tablet-mode switch. Verify finger/pen separation across folding and resume.

After setting up the Copr project and credentials, enable its
`fedora-45-x86_64` chroot and submit the source RPM:

```sh
copr-cli build OWNER/PROJECT dist/fedora/touchscreen-tablet-toggle-0.1.1-1.fc45.src.rpm
```

Verify the resulting **Copr-signed build** from a clean Fedora system:

```sh
sudo dnf copr enable OWNER/PROJECT
sudo dnf install touchscreen-tablet-toggle
sudo systemctl enable --now touchscreen-tablet-toggle.service
```

Then enable the GNOME extension per user as in the README. Publish the actual
repository name after verification. A local release-candidate RPM is unsigned.
