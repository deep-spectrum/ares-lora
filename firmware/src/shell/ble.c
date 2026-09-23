/**
 * @file ble.c
 *
 * @brief
 *
 * @date 9/22/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ble/ble.h>
#include <zephyr/shell/shell.h>

enum {
    BLE_ON,
    BLE_OFF,
    BLE_GET,
};

static int cmd_ble_state(const struct shell *sh, size_t argc, char **argv,
                         void *data) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    int ret, state = (int)data;

    switch (state) {
    case BLE_ON: {
        ret = ares_enable_ble();
        break;
    }
    case BLE_OFF: {
        ret = ares_disable_ble();
        break;
    }
    case BLE_GET: {
        bool result = ares_ble_enabled();
        shell_print(sh, "BLE is %s", result ? "on" : "off");
        return 0;
    }
    default: {
        shell_error(sh, "Shell reached an unreachable point");
        return 1;
    }
    }

    if (ret < 0) {
        shell_error(sh, "Failed: %d", ret);
        return 1;
    }

    return 0;
}

SHELL_SUBCMD_DICT_SET_CREATE(sub_state, cmd_ble_state,
                             (on, BLE_ON, "Turn BLE on"),
                             (off, BLE_OFF, "Turn BLE off"),
                             (get, BLE_GET, "Get BLE state"));

#define BLE_STATE_HELP "Set or get the BLE state"

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ble,
                               SHELL_CMD(state, &sub_state, BLE_STATE_HELP,
                                         NULL),
                               SHELL_SUBCMD_SET_END, );

SHELL_CMD_REGISTER(ble, &sub_ble, "BLE commands", NULL);
