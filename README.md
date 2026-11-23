# Tablet-only Touchscreen

A small GNOME utility for convertible laptops. It turns off finger touch while
you use the laptop with its keyboard and turns it back on when you fold it
into tablet mode. The pen and touchpad stay independent.

## Quick start

1. Download the `.deb` for **Ubuntu 26.04** or the `.x86_64.rpm` for
   **Fedora Workstation 45** from [Releases](https://github.com/Omniptizator/touchscreen-tablet-toggle/releases).
2. From the download directory, install the package for your system:

   **Ubuntu**
   ```sh
   sudo apt install ./touchscreen-tablet-toggle_0.2.0_amd64.deb
   ```
   **Fedora**
   ```sh
   sudo dnf install ./touchscreen-tablet-toggle-0.2.0-1.fc45.x86_64.rpm
   sudo systemctl enable --now touchscreen-tablet-toggle.service
   ```
3. Log out and back in, then enable **Tablet-only Touchscreen** in GNOME
   Extensions. Open **Quick Settings** and turn on **Tablet-only touchscreen**.
   Switch the tile off at any time to restore normal touch in both modes.

Requires an **Intel Virtual Switches** tablet-mode sensor and a separate
internal I²C or SPI finger touchscreen. Verified on an HP Spectre x360
15-bl012dx running Ubuntu. See the [guide](docs/guide.md) for hardware
discovery, troubleshooting, and building from source.
