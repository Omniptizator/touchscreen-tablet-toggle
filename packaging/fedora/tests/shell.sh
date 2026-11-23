#!/usr/bin/env bash
# Run from the source directory inside the runtime container under dbus-run-session.
set -euo pipefail
export DBUS_SYSTEM_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:?run under dbus-run-session}"
export GIO_USE_VFS=local GSETTINGS_BACKEND=keyfile LIBGL_ALWAYS_SOFTWARE=1
export HOME XDG_RUNTIME_DIR TTT_FIXTURE
HOME=$(mktemp -d)
XDG_RUNTIME_DIR=$HOME/run
TTT_FIXTURE=$HOME/fixture
mkdir -m700 "$XDG_RUNTIME_DIR" "$TTT_FIXTURE"
uuid=tablet-only-touchscreen@touchscreen-tablet-toggle
probe=probe@test.invalid
ext=$HOME/.local/share/gnome-shell/extensions/$probe
mkdir -p "$ext"
cp packaging/fedora/tests/probe.js "$ext/extension.js"
echo "{\"uuid\":\"$probe\",\"name\":\"CI probe\",\"description\":\"UI smoke test\",\"shell-version\":[\"50\",\"51\"]}" > "$ext/metadata.json"
touch "$TTT_FIXTURE/allowed"
STATE_DIRECTORY=$TTT_FIXTURE build/mock &
export TTT_MOCK_PID=$!
authority=$PWD/build/authority
(cd "$TTT_FIXTURE" && exec "$authority") &
loginpid=$!
cleanup() {
    [ -f "$TTT_FIXTURE/passed" ] || cat "$HOME/shell.log" >&2
    kill "${shellpid:-}" "$loginpid" "$TTT_MOCK_PID" 2>/dev/null || :
    rm -rf "$HOME"
}
trap cleanup EXIT
gdbus wait --session -t 8 org.freedesktop.login1
gdbus wait --session -t 8 io.github.TouchscreenTabletToggle
gsettings set org.gnome.shell disable-extension-version-validation false
gsettings set org.gnome.shell enabled-extensions "['$uuid', '$probe']"
gnome-shell --headless --virtual-monitor 1024x768 >"$HOME/shell.log" 2>&1 &
shellpid=$!
gdbus wait --session -t 20 org.gnome.Shell
for ((i=0; i<150; i++)); do
    [ ! -f "$TTT_FIXTURE/passed" ] || break
    kill -0 "$shellpid"
    sleep 0.2
done
test -f "$TTT_FIXTURE/passed"
test "$(cat "$TTT_FIXTURE/enabled")" = 0
# The extensions helper can be D-Bus activated outside the compositor.
# Give it the headless compositor's socket, not the host desktop's display.
for socket in "$XDG_RUNTIME_DIR"/wayland-*; do
    if [[ -S $socket ]]; then
        export WAYLAND_DISPLAY=${socket##*/}
        dbus-update-activation-environment HOME XDG_RUNTIME_DIR WAYLAND_DISPLAY GSETTINGS_BACKEND
        break
    fi
done
gnome-extensions disable "$uuid"
gnome-extensions enable "$uuid"
gnome-extensions info "$uuid" | grep 'State: ACTIVE'
echo "GNOME Shell: tile clicks, laptop/tablet subtitles, disable/re-enable passed."
