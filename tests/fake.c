/* Link-time replacement for linux.c only. The production binary has no test switches. */
#include "../service/input.h"
#include <glib-unix.h>
#include <fcntl.h>
#include <unistd.h>

int posture, generation=1, screens=1, switches=1, grabs, releases, scans;
gboolean busy;
static int pipefd[2];
struct Input { gboolean sensor; };
void scan_devices(Scan *s,gboolean print)
{
    scans++;
    s->screens=screens;
    s->switches=switches;
    s->sw=(Device){g_strdup("switch"),g_strdup("/switch"),1,1};
    s->touch=(Device){g_strdup("event9"),g_strdup("/screen"),generation,9};
}
Input *input_open(const Device *d,gboolean sensor,const char **error)
{
    if (!sensor && busy) { *error="busy"; return NULL; }
    Input *i=g_new(Input,1);
    i->sensor=sensor;
    if (!sensor) grabs++;
    return i;
}
void input_close(Input *i) { if (!i->sensor) releases++; g_free(i); }
int input_fd(Input *i) { return pipefd[0]; }
int input_mode(Input *i)
{
    const char *dir=g_getenv("STATE_DIRECTORY");
    if (dir) {
        g_autofree char *p=g_build_filename(dir,"silent-tablet",NULL);
        if (g_file_test(p,G_FILE_TEST_EXISTS)) return 1;
    }
    return posture;
}
gboolean input_drain(Input *i) { char c; return read(pipefd[0],&c,1)==1; }
void report_mode(int mode)
{
    posture=mode;
    g_assert_cmpint(write(pipefd[1],"x",1),==,1);
}
static gboolean fold(gpointer unused) { report_mode(!posture); return G_SOURCE_CONTINUE; }
gboolean monitor_start(void)
{
    g_unix_signal_add(SIGUSR1,fold,NULL);
    return g_unix_open_pipe(pipefd,FD_CLOEXEC,NULL);
}
void monitor_stop(void) { close(pipefd[0]); close(pipefd[1]); }
