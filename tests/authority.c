#include <gio/gio.h>

static const char xml[]="<node><interface name='org.freedesktop.PolicyKit1.Authority'>"
"<method name='CheckAuthorization'><arg type='(sa{sv})'/><arg type='s'/>"
"<arg type='a{ss}'/><arg type='u'/><arg type='s'/><arg type='(bba{ss})' direction='out'/>"
"</method></interface><interface name='org.freedesktop.login1.Manager'>"
"<method name='ListSessions'><arg type='a(susso)' direction='out'/></method>"
"<method name='TestResume'/>"
"<signal name='PrepareForSleep'><arg type='b'/></signal></interface></node>";

static void method(GDBusConnection *c,const char *sender,const char *path,const char *iface,
                   const char *method,GVariant *args,GDBusMethodInvocation *inv,gpointer unused)
{
    if (!strcmp(method,"TestResume")) {
        g_dbus_connection_emit_signal(c,NULL,path,iface,"PrepareForSleep",
                                      g_variant_new("(b)",FALSE),NULL);
        g_dbus_method_invocation_return_value(inv,NULL);
        return;
    }
    if (!strcmp(method,"ListSessions")) {
        g_dbus_method_invocation_return_value(inv,g_variant_new("(a(susso))",NULL));
        return;
    }
    const char *kind,*action,*client=NULL;
    guint flags;
    g_autoptr(GVariant) subject=NULL;
    g_variant_get(args,"((&s@a{sv})&s@a{ss}u&s)",&kind,&subject,&action,NULL,&flags,NULL);
    gboolean ok=!strcmp(action,"io.github.touchscreen-tablet-toggle.configure") &&
        !strcmp(kind,"system-bus-name") && g_variant_lookup(subject,"name","&s",&client) &&
        g_dbus_is_unique_name(client) && !flags && g_file_test("allowed",G_FILE_TEST_EXISTS);
    g_dbus_method_invocation_return_value(inv,g_variant_new("((bba{ss}))",ok,FALSE,NULL));
}
int main(void)
{
    GDBusConnection *c=g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);
    g_assert_nonnull(c);
    GDBusNodeInfo *info=g_dbus_node_info_new_for_xml(xml,NULL);
    const GDBusInterfaceVTable vt={.method_call=method};
    const char *names[]={"org.freedesktop.PolicyKit1","org.freedesktop.login1"};
    const char *paths[]={"/org/freedesktop/PolicyKit1/Authority","/org/freedesktop/login1"};
    for (unsigned n=0;n<2;n++) {
        g_assert_true(g_dbus_connection_register_object(c,paths[n],info->interfaces[n],&vt,NULL,NULL,NULL)>0);
        g_bus_own_name_on_connection(c,names[n],0,NULL,NULL,NULL,NULL);
    }
    g_main_loop_run(g_main_loop_new(NULL,FALSE));
}
