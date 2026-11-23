B := build
PKGS := gio-unix-2.0 libudev libevdev
CPPFLAGS += $(shell pkg-config --cflags $(PKGS))
LDLIBS += $(shell pkg-config --libs $(PKGS))
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -Wno-unused-parameter
daemon := service/hardware.c service/main.c

.PHONY: all fixtures check bench clean
all: $(B)/touchscreen-tablet-toggle
fixtures: $(B)/mock $(B)/authority $(B)/bus $(B)/transitions $(B)/policy
$(B):
	mkdir -p $@
$(B)/touchscreen-tablet-toggle: $(daemon) service/linux.c service/select.c service/select.h service/input.h | $(B)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS) $(LDLIBS)
$(B)/mock: $(daemon) tests/fake.c service/input.h | $(B)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS) $(LDLIBS)
$(B)/transitions: tests/transitions.c tests/fake.c service/hardware.c service/input.h | $(B)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS) $(LDLIBS)
$(B)/%: tests/%.c | $(B)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)
$(B)/policy: tests/policy.c service/select.c service/select.h | $(B)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS) $(LDLIBS)
check: all fixtures
	$(B)/policy
	$(B)/transitions
	dbus-run-session -- bash tests/session.sh check
bench: all fixtures
	$(B)/transitions
	dbus-run-session -- bash tests/session.sh bench
clean:
	rm -rf $(B)
