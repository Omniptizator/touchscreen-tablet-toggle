#include "select.h"

gboolean is_switch(const char *name, const char *bits)
{
    if (!bits || g_strcmp0(name,SENSOR)) return FALSE;
    const char *last = strrchr(bits,' ');
    return !!(g_ascii_strtoull(last ? last+1 : bits,NULL,16) & (1u << SW_TABLET_MODE));
}
gboolean is_screen(const char *bus, gboolean finger, gboolean composite)
{
    return finger && !composite && (!g_strcmp0(bus,"0018") || !g_strcmp0(bus,"001c"));
}
gboolean isolated_finger(const struct libevdev *ev)
{
    int bus = libevdev_get_id_bustype(ev);
    return (bus == BUS_I2C || bus == BUS_SPI) &&
           libevdev_has_property(ev, INPUT_PROP_DIRECT) &&
           libevdev_has_event_code(ev, EV_ABS, ABS_MT_POSITION_X) &&
           !libevdev_has_event_code(ev, EV_KEY, BTN_TOOL_PEN) &&
           !libevdev_has_event_code(ev, EV_KEY, BTN_TOOL_RUBBER) &&
           !libevdev_has_event_code(ev, EV_KEY, BTN_STYLUS) &&
           !libevdev_has_event_code(ev, EV_KEY, BTN_STYLUS2) &&
           !libevdev_has_event_code(ev, EV_KEY, BTN_STYLUS3);
}
