#pragma once
#include <glib.h>
#include <libevdev/libevdev.h>
#define SENSOR "Intel Virtual Switches"
gboolean is_switch(const char *name, const char *bits);
gboolean is_screen(const char *bus, gboolean finger, gboolean composite);
gboolean isolated_finger(const struct libevdev *ev);
