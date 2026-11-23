#include "../service/input.h"
extern int posture,generation,screens,switches,grabs,releases,scans;
extern gboolean busy;
void report_mode(int);
void changed(void) {}
int main(void)
{
    g_assert_true(monitor_start());
    status.enabled=TRUE;
    refresh();
    gint64 start = g_get_monotonic_time();
    for (int n=0;n<30;n++) refresh(); /* unrelated add/remove discovery results */
    g_assert_cmpint(grabs,==,1);
    g_assert_cmpint(releases,==,0);
    g_print("30 simulated rescans: %" G_GINT64_FORMAT " us, 0 releases/regrabs\n",
            g_get_monotonic_time()-start);
    generation++; /* same sysfs name and devnum, different inode */
    refresh();
    g_assert_cmpint(grabs,==,2);
    g_assert_cmpint(releases,==,1);
    removed("/mouse");
    refresh();
    g_assert_cmpint(releases,==,1);
    removed("/screen");
    g_assert_false(status.blocked);
    refresh();
    g_assert_cmpint(grabs,==,3);
    start = g_get_monotonic_time();
    for (int n=0;n<1000;n++) {
        report_mode(0);
        g_main_context_iteration(NULL,TRUE);
    }
    g_assert_cmpint(releases,==,2);
    g_print("1000 duplicate switch reports: %" G_GINT64_FORMAT " us, 0 releases/regrabs\n",
            g_get_monotonic_time()-start);
    start = g_get_monotonic_time();
    for (int n=0;n<100;n++) {
        report_mode(!(n%2));
        g_main_context_iteration(NULL,TRUE);
        g_assert_cmpint(status.blocked,==,n%2);
    }
    g_print("100 alternating posture reports: %" G_GINT64_FORMAT " us, 50 releases/regrabs\n",
            g_get_monotonic_time()-start);
    report_mode(1);
    g_main_context_iteration(NULL,TRUE);
    g_assert_false(status.blocked);
    posture=0; /* resume without a switch event */
    refresh();
    g_assert_true(status.blocked);
    screens=2;
    refresh();
    g_assert_false(status.blocked);
    g_assert_cmpint(status.mode,==,-1);
    screens=1; switches=0;
    refresh();
    g_assert_false(status.blocked);
    switches=1; busy=TRUE;
    refresh();
    g_assert_false(status.blocked);
    g_assert_cmpstr(status.problem,==,"busy");
    busy=FALSE;
    apply();
    g_assert_true(status.blocked);
    status.enabled=FALSE;
    apply();
    g_assert_false(status.blocked);
    reset(); monitor_stop();
    g_assert_cmpint(grabs,==,releases);
    return 0;
}
