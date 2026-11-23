#include "input.h"
#include "select.h"
#include <glib-unix.h>
#include <libudev.h>
#include <libevdev/libevdev.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

struct Input { int fd; struct libevdev *ev; };
static struct udev *context;
static struct udev_monitor *mon;
static guint watch;

static gboolean tagged(struct udev_device *d, const char *tag)
{
    return !g_strcmp0(udev_device_get_property_value(d, tag), "1");
}

static void record(Device *out, unsigned *count, struct udev_device *d,
                   gboolean print, const char *label)
{
    const char *node = udev_device_get_devnode(d);
    struct stat st;
    if (print) g_print("%s: %s\n", label, node);
    if (++*count != 1) return;
    out->path = g_strdup(node);
    out->sys = g_strdup(udev_device_get_syspath(d));
    out->dev = udev_device_get_devnum(d);
    if (stat(node, &st) == 0 && st.st_rdev == out->dev) out->inode = st.st_ino;
}

void scan_devices(Scan *s, gboolean print)
{
    struct udev *u = context ? udev_ref(context) : udev_new();
    if (!u) return;
    struct udev_enumerate *e = udev_enumerate_new(u);
    if (!e || udev_enumerate_add_match_subsystem(e, "input") < 0 ||
        udev_enumerate_add_match_sysname(e, "event*") < 0 ||
        udev_enumerate_scan_devices(e) < 0) goto out;
    struct udev_list_entry *ent;
    udev_list_entry_foreach(ent, udev_enumerate_get_list_entry(e)) {
        struct udev_device *d = udev_device_new_from_syspath(u, udev_list_entry_get_name(ent));
        struct udev_device *p = d ? udev_device_get_parent(d) : NULL;
        if (p && udev_device_get_devnode(d)) {
            const char *bits = udev_device_get_sysattr_value(p, "capabilities/sw");
            const char *bus = udev_device_get_sysattr_value(p, "id/bustype");
            if (is_switch(udev_device_get_sysattr_value(p,"name"),bits))
                record(&s->sw, &s->switches, d, print, "Switch");
            if (is_screen(bus, tagged(d,"ID_INPUT_TOUCHSCREEN"),
                tagged(d,"ID_INPUT_TABLET") || tagged(d,"ID_INPUT_TABLET_PAD") ||
                tagged(d,"ID_INPUT_TOUCHPAD") || tagged(d,"ID_INPUT_MOUSE") ||
                tagged(d,"ID_INPUT_KEYBOARD")))
                record(&s->touch, &s->screens, d, print, "Touchscreen");
        }
        udev_device_unref(d);
    }
out:
    udev_enumerate_unref(e);
    udev_unref(u);
}

Input *input_open(const Device *d, gboolean sensor, const char **error)
{
    struct stat st;
    Input *i = g_new0(Input, 1);
    i->fd = open(d->path, O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
    *error = "Input device unavailable or replaced";
    if (i->fd < 0 || fstat(i->fd,&st) || !S_ISCHR(st.st_mode) ||
        st.st_ino != d->inode || st.st_rdev != d->dev ||
        libevdev_new_from_fd(i->fd, &i->ev) < 0) goto bad;
    if (sensor) {
        *error = "Unsupported tablet-mode switch";
        if (g_strcmp0(libevdev_get_name(i->ev), SENSOR) ||
            !libevdev_has_event_code(i->ev, EV_SW, SW_TABLET_MODE)) goto bad;
    } else {
        *error = "No isolated multitouch finger device";
        if (!isolated_finger(i->ev)) goto bad;
        *error = "Touchscreen is busy or cannot be grabbed";
        if (libevdev_grab(i->ev, LIBEVDEV_GRAB) < 0) goto bad;
    }
    *error = "";
    return i;
bad:
    input_close(i);
    return NULL;
}

int input_fd(Input *i) { return i->fd; }
int input_mode(Input *i)
{
    unsigned long bits = 0;
    return ioctl(i->fd, EVIOCGSW(sizeof bits), &bits) < 0 ? -1 :
           !!(bits & (1ul << SW_TABLET_MODE));
}
gboolean input_drain(Input *i)
{
    struct input_event event;
    int rc;
    while ((rc = libevdev_next_event(i->ev, LIBEVDEV_READ_FLAG_NORMAL, &event)) >= 0) {
        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            do rc = libevdev_next_event(i->ev, LIBEVDEV_READ_FLAG_SYNC, &event);
            while (rc >= 0);
            if (rc != -EAGAIN) return FALSE;
        }
    }
    return rc == -EAGAIN;
}
void input_close(Input *i)
{
    if (i->ev) libevdev_free(i->ev);
    if (i->fd >= 0) close(i->fd);
    g_free(i);
}

static gboolean hotplug(int fd, GIOCondition cond, gpointer unused)
{
    if (cond & (G_IO_ERR|G_IO_HUP)) g_error("Lost input monitor");
    struct udev_device *d;
    while ((d = udev_monitor_receive_device(mon))) {
        if (!g_strcmp0(udev_device_get_action(d), "remove"))
            removed(udev_device_get_syspath(d));
        udev_device_unref(d);
    }
    refresh();
    return G_SOURCE_CONTINUE;
}
gboolean monitor_start(void)
{
    context = udev_new();
    if (!context) return FALSE;
    mon = udev_monitor_new_from_netlink(context, "udev");
    if (!mon || udev_monitor_filter_add_match_subsystem_devtype(mon,"input",NULL) < 0 ||
        udev_monitor_enable_receiving(mon) < 0) return FALSE;
    watch = g_unix_fd_add(udev_monitor_get_fd(mon), G_IO_IN|G_IO_ERR|G_IO_HUP, hotplug, NULL);
    return TRUE;
}
void monitor_stop(void)
{
    g_clear_handle_id(&watch, g_source_remove);
    udev_monitor_unref(mon);
    udev_unref(context);
}
