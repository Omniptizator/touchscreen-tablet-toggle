# Resource profile: Ubuntu on the HP Spectre

Measured 2026-11-23 on the installed `touchscreen-tablet-toggle` 0.1.0.
The installed service Python files matched the working tree. The live policy
was enabled, the Intel switch was in laptop posture, and finger touch was
blocked. No live settings or input devices were changed.

## Five-minute live idle sample

At six snapshots 60 seconds apart, the unit's cumulative CPU time stayed at
198,661 µs and memory stayed at 17,252,352 bytes (16.45 MiB). None of the
four threads gained a voluntary or involuntary context switch. The D-Bus
monitor saw no signals from the service object. With the policy active and
touch blocked, there was no measurable idle CPU cost during this window.

## Memory

The live unit used **16.45 MiB** according to cgroup v2 `memory.current`
(16.00 MiB anonymous, about 0.45 MiB kernel memory). The process had four
threads and **32.24 MiB RSS**; about half of RSS was file-backed. RSS and
cgroup usage are different accounting methods, not figures to add together.

Direct PSS and descriptor access were restricted for the root-owned daemon.
An unprivileged copy of the installed daemon on a private D-Bus, with the
policy off and no readable switch, used **17.94 MiB PSS**, **15.92 MiB
private memory**, four threads and 11 open descriptors. This is a proxy
measurement, not the live daemon's PSS. A fresh interpreter reached about
13 MiB private memory just importing Gio/GLib and about 16 MiB after importing
the service and input libraries. The steady memory cost is mostly the runtime.

## Event handling

The isolated benchmark used a private D-Bus, actual read-only udev discovery
on the laptop, simulated switch values, and stand-ins for input handles. It
called the service handlers directly; timings do not include physical input,
Wayland rendering, or the desktop's handling of a notification. Results below
are medians across three interleaved runs.

| Workload | Current service | Experimental guard |
| --- | --- | --- |
| 30 distinct unrelated mouse-add refreshes, laptop mode | 30 scans, 30 release/regrab cycles, 30 signals; 362 ms process CPU | Skip a positively identified unrelated add: 0 scans/releases/signals, <0.1 ms CPU |
| 1,000 unchanged-state switch callbacks | 1,000 D-Bus property signals, 286 ms CPU | Cache the last published status: 0 signals, 3.1 ms CPU |
| 100 actual simulated posture changes | 100 signals, 50 grabs, 50 releases; 28 ms CPU | Same counts; 32 ms CPU |
| 100 posture changes while policy off | 100 signals, no grabs | Same; monitoring still exposes the current posture |

`refresh()` releases the existing grab *before* enumerating input. The
median simulated unblocked interval for an unrelated hotplug was **11.5 ms**.
That identifies a potential window for a phantom tap; it is not evidence
that one occurred. Several udev events may already coalesce into one refresh.

In a separate ten-second idle comparison per configuration, the current and
experimental processes, both enabled and disabled, each used <0.06 ms CPU,
had one aggregate context switch, and used about 18.5 MiB PSS (including the
benchmark harness). A context switch is a scheduling indicator, not a direct
measure of platform power or hardware wakeups.

## Recommendation

Keep the event-driven service. If changing it, prioritize **retaining the
grab during unrelated hotplug** and suppressing duplicate property signals.
Preserve full discovery for ambiguous events and fail open when the selected
switch or screen disappears. Do not use the reusable `eventN` path alone as
device identity. These optimizations reduce event work, not resident memory.
For a strict memory budget, designing a smaller native backend or actually
exiting while disabled would have a larger effect; either is a separate
design change and needs a memory target to justify it.
