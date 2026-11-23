#pragma once
#include <glib.h>
#include <sys/types.h>

/* The inode distinguishes successive instances of the same devnode. */
typedef struct { char *path, *sys; ino_t inode; dev_t dev; } Device;
typedef struct { Device sw, touch; unsigned switches, screens; } Scan;
typedef struct Input Input;
typedef struct {
    gboolean enabled, blocked;
    int mode;
    const char *problem;
} Status;

void scan_devices(Scan *s, gboolean print);
Input *input_open(const Device *d, gboolean sensor, const char **error);
int input_fd(Input *i);
int input_mode(Input *i);
gboolean input_drain(Input *i);
void input_close(Input *i);
gboolean monitor_start(void);
void monitor_stop(void);

extern Status status;
void changed(void);
void refresh(void);
void removed(const char *sys);
void apply(void);
void reset(void);
