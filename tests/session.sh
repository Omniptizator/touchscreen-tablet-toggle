#!/usr/bin/env bash
set -eu
root=$PWD
export STATE_DIRECTORY=$(mktemp -d) DBUS_SYSTEM_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:?}" GIO_USE_VFS=local
cd "$STATE_DIRECTORY"
trap 'kill ${pid:-} ${auth:-} 2>/dev/null || :; wait || :; rm -rf "$STATE_DIRECTORY"' EXIT
if [[ $1 == bench ]]; then
    for node in /dev/input/event*; do
        if [[ -r $node ]]; then
            echo "Benchmark needs isolated input devices; use a container without /dev/input." >&2
            exit 1
        fi
    done
fi
start() {
    if [[ $1 == bench ]]; then
        "${BENCH_DAEMON:-$root/build/touchscreen-tablet-toggle}" &
    else
        "$root/build/mock" &
    fi
    pid=$!
    gdbus wait --session -t 5 io.github.TouchscreenTabletToggle
}
"$root/build/authority" &
auth=$!
gdbus wait --session -t 5 org.freedesktop.PolicyKit1
start "$1"
if [[ $1 == bench ]]; then
    touch allowed
    sleep 1
    sample() {
        awk '/^Pss:|^Private_(Dirty|Clean):/{printf "%s%s ",$1,$2}' "/proc/$pid/smaps_rollup"
        awk '{print "CPU_ticks=" $14+$15}' "/proc/$pid/stat"
        awk '/ctxt_switches:/{s+=$2}END{print "context_switches=" s}' /proc/"$pid"/task/*/status
    }
    sample
    sleep "${IDLE_SECONDS:-300}"
    sample
    "$root/build/bus" bench
    sample
else
    "$root/build/bus" deny
    gjs -m "$root/tests/client.js"
    test "$(cat enabled)" = 1
    kill "$pid"; wait "$pid"; pid=
    start "$1"
    "$root/build/bus" restart
    "$root/build/bus" resume
    "$root/build/bus" notifications
    "$root/build/bus" write-error
fi
kill "$pid"; wait "$pid"; pid=
