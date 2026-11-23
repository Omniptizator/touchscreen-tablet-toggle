#include "input.h"
#include <glib-unix.h>

Status status = {.mode = -1, .problem = ""};
static Device sensor, target;
static Input *sw, *held;
static guint watch;

static void clear(Device *d)
{
    g_free(d->path);
    g_free(d->sys);
    *d = (Device){0};
}

static gboolean same(const Device *a, const Device *b)
{
    return a->inode && a->inode == b->inode && a->dev == b->dev &&
           !g_strcmp0(a->sys, b->sys);
}

static void release(void)
{
    if (held) input_close(held);
    held = NULL;
    status.blocked = FALSE;
}

void reset(void)
{
    release();
    g_clear_handle_id(&watch, g_source_remove);
    if (sw) input_close(sw);
    sw = NULL;
    clear(&sensor);
    clear(&target);
    status.mode = -1;
}

static void fail(const char *reason)
{
    reset();
    status.problem = reason;
    changed();
}

void removed(const char *sys)
{
    if (sys && (!g_strcmp0(sys, sensor.sys) || !g_strcmp0(sys, target.sys)))
        fail("Input device disconnected");
}

void apply(void)
{
    if (sw) {
        status.problem = "";
        if (!status.enabled || status.mode != 0) release();
        else if (!held) held = input_open(&target, FALSE, &status.problem);
        status.blocked = held != NULL;
    }
    changed();
}

static gboolean events(gint fd, GIOCondition cond, gpointer unused)
{
    if ((cond & (G_IO_ERR | G_IO_HUP)) || !input_drain(sw) ||
        (status.mode = input_mode(sw)) < 0) {
        watch = 0;
        fail("Tablet-mode switch disconnected");
        return G_SOURCE_REMOVE;
    }
    apply();
    return G_SOURCE_CONTINUE;
}

void refresh(void)
{
    Scan s = {0};
    const char *error = NULL;
    scan_devices(&s, FALSE);
    if (s.switches != 1 || s.screens != 1) {
        fail(s.switches != 1 ? "No unique Intel tablet-mode switch" :
                               "No unique internal finger touchscreen");
        goto out;
    }
    if (!same(&sensor, &s.sw)) {
        reset();
        sw = input_open(&s.sw, TRUE, &error);
        if (!sw) { fail(error); goto out; }
        sensor = s.sw;
        s.sw = (Device){0};
        watch = g_unix_fd_add(input_fd(sw), G_IO_IN | G_IO_ERR | G_IO_HUP, events, NULL);
    }
    if (!same(&target, &s.touch)) {
        release();
        clear(&target);
        target = s.touch;
        s.touch = (Device){0};
    }
    status.mode = input_mode(sw); /* query kernel, including after resume */
    if (status.mode < 0) fail("Cannot read tablet posture");
    else apply();
out:
    clear(&s.sw);
    clear(&s.touch);
}

void discover_print(void)
{
    Scan s = {0};
    scan_devices(&s, TRUE);
    g_print("%u switch candidate(s), %u touchscreen candidate(s)\n",s.switches,s.screens);
    clear(&s.sw);
    clear(&s.touch);
}
