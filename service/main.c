#include "input.h"
#include <gio/gio.h>
#include <glib-unix.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define NAME "io.github.TouchscreenTabletToggle"
#define PATH "/io/github/TouchscreenTabletToggle"
#define AUTH "org.freedesktop.PolicyKit1.Authority"
static const char xml[] = "<node><interface name='" NAME "'>"
    "<method name='SetEnabled'><arg type='b' direction='in'/></method>"
    "<property name='Enabled' type='b' access='read'/>"
    "<property name='TabletMode' type='i' access='read'/>"
    "<property name='Blocked' type='b' access='read'/>"
    "<property name='Problem' type='s' access='read'/></interface></node>";
static GDBusConnection *bus;
static char *state;
static GMainLoop *loop;
static gboolean acquired;
static Status prev;

static GVariant *get(GDBusConnection *c,const char *sender,const char *path,const char *iface,
                     const char *prop,GError **e,gpointer data)
{
    if (!strcmp(prop,"Enabled")) return g_variant_new_boolean(status.enabled);
    if (!strcmp(prop,"Blocked")) return g_variant_new_boolean(status.blocked);
    if (!strcmp(prop,"TabletMode")) return g_variant_new_int32(status.mode);
    return g_variant_new_string(status.problem);
}

void changed(void)
{
    if (!acquired) return;
    if (prev.enabled == status.enabled && prev.blocked == status.blocked &&
        prev.mode == status.mode && !g_strcmp0(prev.problem,status.problem)) return;
    GVariantBuilder b;
    g_variant_builder_init(&b,G_VARIANT_TYPE_VARDICT);
#define ADD(prop) g_variant_builder_add(&b,"{sv}",prop,get(NULL,NULL,NULL,NULL,prop,NULL,NULL))
    if (prev.enabled != status.enabled) ADD("Enabled");
    if (prev.blocked != status.blocked) ADD("Blocked");
    if (prev.mode != status.mode) ADD("TabletMode");
    if (g_strcmp0(prev.problem,status.problem)) ADD("Problem");
    prev = status; /* problem strings are static */
    g_dbus_connection_emit_signal(bus,NULL,PATH,"org.freedesktop.DBus.Properties",
        "PropertiesChanged",g_variant_new("(sa{sv}as)",NAME,&b,NULL),NULL);
}

/* Match the original atomic rename semantics. Synchronous disk flushes
 * would stall switch event handling on the main thread. */
static gboolean save_enabled(gboolean enabled,GError **error)
{
    g_autofree char *tmp = g_strconcat(state,".XXXXXX",NULL);
    int fd = g_mkstemp_full(tmp,O_RDWR|O_CLOEXEC,0600);
    if (fd < 0) {
        g_set_error_literal(error,G_FILE_ERROR,g_file_error_from_errno(errno),g_strerror(errno));
        return FALSE;
    }
    const char *data = enabled ? "1\n" : "0\n";
    int err = 0;
    for (size_t pos = 0; pos < 2;) {
        ssize_t n = write(fd,data+pos,2-pos);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { err = n < 0 ? errno : EIO; break; }
        pos += n;
    }
    if (close(fd) < 0 && !err) err = errno;
    if (!err && g_rename(tmp,state) == 0) return TRUE;
    if (!err) err = errno;
    g_unlink(tmp);
    g_set_error_literal(error,G_FILE_ERROR,g_file_error_from_errno(err),g_strerror(err));
    return FALSE;
}

static void authorized(GObject *obj,GAsyncResult *res,gpointer data)
{
    GDBusMethodInvocation *inv = data;
    g_autoptr(GVariant) r = g_dbus_connection_call_finish(bus,res,NULL);
    gboolean allowed = FALSE, enabled;
    if (r) g_variant_get_child(r,0,"(bb@a{ss})",&allowed,NULL,NULL);
    if (!allowed) {
        g_dbus_method_invocation_return_dbus_error(inv,"org.freedesktop.DBus.Error.AccessDenied",
                                                  "An active local session is required");
        return;
    }
    g_variant_get(g_dbus_method_invocation_get_parameters(inv),"(b)",&enabled);
    g_autoptr(GError) e = NULL;
    if (!save_enabled(enabled,&e)) {
        g_dbus_method_invocation_return_dbus_error(inv,NAME ".Error",e->message);
        return;
    }
    status.enabled = enabled;
    apply();
    g_dbus_method_invocation_return_value(inv,NULL);
}

static void method(GDBusConnection *c,const char *sender,const char *path,const char *iface,
                   const char *name,GVariant *args,GDBusMethodInvocation *inv,gpointer data)
{
    GVariantBuilder b;
    g_variant_builder_init(&b,G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&b,"{sv}","name",g_variant_new_string(sender));
    g_dbus_connection_call(c,"org.freedesktop.PolicyKit1","/org/freedesktop/PolicyKit1/Authority",
        AUTH,"CheckAuthorization",g_variant_new("((sa{sv})sa{ss}us)","system-bus-name",&b,
        "io.github.touchscreen-tablet-toggle.configure",NULL,0u,""),G_VARIANT_TYPE("((bba{ss}))"),
        0,5000,NULL,authorized,inv);
}

static void ready(GDBusConnection *c,const char *name,gpointer data)
{
    acquired = TRUE;
    if (!monitor_start()) g_error("Cannot start input monitor");
    refresh();
}
static void lost(GDBusConnection *c,const char *name,gpointer data)
{
    acquired = FALSE;
    g_main_loop_quit(loop);
}
static gboolean quit(gpointer data) { g_main_loop_quit(loop); return G_SOURCE_REMOVE; }
static void sleep_signal(GDBusConnection *c,const char *sender,const char *path,const char *iface,
                         const char *name,GVariant *args,gpointer data)
{
    gboolean sleeping;
    if (!acquired || !g_variant_is_of_type(args,G_VARIANT_TYPE("(b)"))) return;
    g_variant_get(args,"(b)",&sleeping);
    if (!sleeping) refresh();
}
void discover_print(void);
int main(int argc,char **argv)
{
    if (argc == 2 && !strcmp(argv[1],"--discover")) { discover_print(); return 0; }
    if (argc != 1 || !g_getenv("STATE_DIRECTORY")) return 2;
    state = g_build_filename(g_getenv("STATE_DIRECTORY"),"enabled",NULL);
    g_autofree char *saved = NULL;
    g_autoptr(GError) e = NULL;
    if (g_file_get_contents(state,&saved,NULL,&e)) status.enabled = !strcmp(g_strstrip(saved),"1");
    else if (!g_error_matches(e,G_FILE_ERROR,G_FILE_ERROR_NOENT)) g_error("%s",e->message);
    g_clear_error(&e);
    bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM,NULL,&e);
    if (!bus) g_error("%s",e->message);
    loop = g_main_loop_new(NULL,FALSE);
    g_autoptr(GDBusNodeInfo) info = g_dbus_node_info_new_for_xml(xml,NULL);
    const GDBusInterfaceVTable vt = {.method_call=method,.get_property=get};
    if (!g_dbus_connection_register_object(bus,PATH,info->interfaces[0],&vt,NULL,NULL,&e))
        g_error("%s",e->message);
    g_dbus_connection_signal_subscribe(bus,"org.freedesktop.login1","org.freedesktop.login1.Manager",
        "PrepareForSleep","/org/freedesktop/login1",NULL,0,sleep_signal,NULL,NULL);
    guint owner = g_bus_own_name_on_connection(bus,NAME,G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE,
                                               ready,lost,NULL,NULL);
    g_unix_signal_add(SIGTERM,quit,NULL);
    g_unix_signal_add(SIGINT,quit,NULL);
    g_main_loop_run(loop);
    monitor_stop();
    reset();
    g_bus_unown_name(owner);
    return acquired ? 0 : 1;
}
