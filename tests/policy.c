#include "../service/select.h"
int main(void)
{
    g_assert_true(is_switch(SENSOR,"0 22"));
    g_assert_false(is_switch("HP WMI hotkeys","22"));
    g_assert_false(is_switch(SENSOR,"1"));
    g_assert_true(is_screen("0018",TRUE,FALSE));
    g_assert_true(is_screen("001c",TRUE,FALSE));
    g_assert_false(is_screen("0003",TRUE,FALSE));
    g_assert_false(is_screen("0018",TRUE,TRUE));
    g_assert_false(is_screen("0018",FALSE,FALSE));
    struct libevdev *ev=libevdev_new();
    struct input_absinfo abs={.maximum=100};
    libevdev_set_id_bustype(ev,BUS_I2C);
    libevdev_enable_event_code(ev,EV_ABS,ABS_MT_POSITION_X,&abs);
    g_assert_false(isolated_finger(ev));
    libevdev_enable_property(ev,INPUT_PROP_DIRECT);
    g_assert_true(isolated_finger(ev));
    libevdev_enable_event_code(ev,EV_KEY,BTN_STYLUS,NULL);
    g_assert_false(isolated_finger(ev));
    libevdev_free(ev);
}
