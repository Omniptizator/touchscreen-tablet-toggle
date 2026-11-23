#include <gio/gio.h>
#include <glib/gstdio.h>
#include <unistd.h>

#define NAME "io.github.TouchscreenTabletToggle"
#define PATH "/io/github/TouchscreenTabletToggle"
static GDBusConnection *bus;
static int notifications;
static void notified(GDBusConnection *c,const char *sender,const char *path,const char *iface,
                     const char *name,GVariant *args,gpointer data)
{
    g_autoptr(GVariant) dict=g_variant_get_child_value(args,1);
    gboolean blocked,enabled;
    g_assert_true(g_variant_lookup(dict,"Enabled","b",&enabled));
    g_assert_true(g_variant_lookup(dict,"Blocked","b",&blocked));
    g_assert_false(blocked);
    g_assert_false(enabled);
    g_assert_cmpuint(g_variant_n_children(dict),==,2);
    notifications++;
}

static GVariant *call(const char *iface, const char *method, GVariant *args, GError **e)
{
    return g_dbus_connection_call_sync(bus,NAME,PATH,iface,method,args,NULL,0,5000,NULL,e);
}

int main(int argc, char **argv)
{
    g_assert_cmpint(argc, ==, 2);
    g_autoptr(GError) e = NULL;
    bus = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,&e);
    g_assert_no_error(e);
    if (!strcmp(argv[1],"write-error")) {
        g_assert_cmpint(unlink("enabled"),==,0);
        g_assert_cmpint(g_mkdir("enabled",0700),==,0);
        g_autoptr(GVariant) r=call(NAME,"SetEnabled",g_variant_new("(b)",TRUE),&e);
        g_assert_null(r);
        g_assert_nonnull(e);
        g_autofree char *name=g_dbus_error_get_remote_error(e);
        g_assert_cmpstr(name,==,NAME ".Error");
        g_clear_error(&e);
        r=call("org.freedesktop.DBus.Properties","Get",
               g_variant_new("(ss)",NAME,"Enabled"),&e);
        g_assert_no_error(e);
        g_autoptr(GVariant) value=NULL;
        g_variant_get(r,"(v)",&value);
        g_assert_false(g_variant_get_boolean(value));
        g_autoptr(GDir) dir=g_dir_open(".",0,NULL);
        const char *file;
        while ((file=g_dir_read_name(dir))) g_assert_false(g_str_has_prefix(file,"enabled."));
        return 0;
    }
    if (!strcmp(argv[1],"resume")) {
        for (int tablet=1;tablet>=0;tablet--) {
            if (tablet) g_assert_true(g_file_set_contents("silent-tablet","",0,NULL));
            else g_assert_cmpint(unlink("silent-tablet"),==,0);
            g_autoptr(GVariant) reply=g_dbus_connection_call_sync(bus,"org.freedesktop.login1",
                "/org/freedesktop/login1","org.freedesktop.login1.Manager","TestResume",NULL,
                NULL,0,5000,NULL,&e);
            g_assert_no_error(e);
            gint64 deadline=g_get_monotonic_time()+G_TIME_SPAN_SECOND;
            for (;;) {
                g_autoptr(GVariant) r=call("org.freedesktop.DBus.Properties","Get",
                                          g_variant_new("(ss)",NAME,"Blocked"),&e);
                g_assert_no_error(e);
                g_autoptr(GVariant) value=NULL;
                g_variant_get(r,"(v)",&value);
                if (g_variant_get_boolean(value)==!tablet) break;
                g_assert_true(g_get_monotonic_time()<deadline);
            }
        }
        return 0;
    }
    if (!strcmp(argv[1],"notifications")) {
        g_dbus_connection_signal_subscribe(bus,NAME,"org.freedesktop.DBus.Properties",
            "PropertiesChanged",PATH,NAME,0,notified,NULL,NULL);
        for (int n=0;n<10;n++) {
            g_autoptr(GVariant) r=call(NAME,"SetEnabled",g_variant_new("(b)",FALSE),&e);
            g_assert_no_error(e);
        }
        /* Each setter queues its signal before its reply on the same connection. */
        while (g_main_context_iteration(NULL,FALSE)) {}
        g_assert_cmpint(notifications,==,1);
        return 0;
    }
    if (!strcmp(argv[1],"bench")) {
        gint64 start = g_get_monotonic_time();
        for (int n=0; n<1000; n++) {
            g_autoptr(GVariant) r = call(NAME,"SetEnabled",g_variant_new("(b)",n%2),&e);
            g_assert_no_error(e);
        }
        g_print("1000 alternating SetEnabled calls: %.1f ms\n",(g_get_monotonic_time()-start)/1000.);
        return 0;
    }
    if (!strcmp(argv[1],"restart")) {
        g_autoptr(GVariant) r = call("org.freedesktop.DBus.Properties","Get",
                                    g_variant_new("(ss)",NAME,"Enabled"),&e);
        g_assert_no_error(e);
        g_autoptr(GVariant) value = NULL;
        g_variant_get(r,"(v)",&value);
        g_assert_true(g_variant_get_boolean(value));
        return 0;
    }
    g_autoptr(GVariant) r = call(NAME,"SetEnabled",g_variant_new("(b)",TRUE),&e);
    g_assert_error(e,G_DBUS_ERROR,G_DBUS_ERROR_ACCESS_DENIED);
    g_assert_false(g_file_test("enabled",G_FILE_TEST_EXISTS));
    g_assert_true(g_file_set_contents("allowed","",0,NULL));
    return 0;
}
