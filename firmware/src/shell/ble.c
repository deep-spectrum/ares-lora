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
#include <ble/services/ares_service.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(ble_shell);

static uint64_t u64_configs[3] = {
    [ARES_CONFIG_BANDWIDTH] = 100,
    [ARES_CONFIG_CENTER_FREQ] = 2450000000,
    [ARES_CONFIG_REF_LEVEL] = -20,
};

static uint32_t u32_configs[1] = {
    [ARES_CONFIG_DURATION - ARES_CONFIG_DURATION] = 30,
};

struct config_buffer {
    char config[1024];
    size_t len;
};

static struct config_buffer str_configs[1] = {
    [ARES_CONFIG_DESCRIPTION - ARES_CONFIG_DESCRIPTION] = {.config = "None",
                                                           .len = 5},
};

static void config_update(uint32_t type, uint64_t value) {
    LOG_INF("config_update: {Thread Name: %s, Thread Prio: %d}",
            k_thread_name_get(k_current_get()),
            k_thread_priority_get(k_current_get()));
    LOG_INF("Configuration Updated. Config ID: %" PRIu32 ", value: %" PRIu64,
            type, value);

    switch (type) {
    case ARES_CONFIG_BANDWIDTH:
    case ARES_CONFIG_CENTER_FREQ:
    case ARES_CONFIG_REF_LEVEL: {
        u64_configs[type] = value;
        break;
    }
    case ARES_CONFIG_DURATION: {
        u32_configs[type - ARES_CONFIG_DURATION] = value;
        break;
    }
    default: {
        LOG_ERR("Invalid config: %" PRIu32, type);
        break;
    }
    }
}

static void description_update(const uint8_t *buf, size_t len) {
    LOG_INF("description_update: {Thread Name: %s, Thread Prio: %d}",
            k_thread_name_get(k_current_get()),
            k_thread_priority_get(k_current_get()));
    LOG_HEXDUMP_INF(buf, len, "New Description");

    memcpy(str_configs[0].config, buf, len);
    str_configs[0].len = len;
}

static void connected(void) {
    LOG_INF("connected: {Thread Name: %s, Thread Prio: %d}",
            k_thread_name_get(k_current_get()),
            k_thread_priority_get(k_current_get()));
}

static void disconnected(void) {
    LOG_INF("disconnected: {Thread Name: %s, Thread Prio: %d}",
            k_thread_name_get(k_current_get()),
            k_thread_priority_get(k_current_get()));
}

static void mtu_changed(size_t new_mtu) {
    LOG_INF("new_mtu: {Thread Name: %s, Thread Prio: %d}",
            k_thread_name_get(k_current_get()),
            k_thread_priority_get(k_current_get()));
    LOG_INF("New MTU size: %" PRIu32, new_mtu);
}

static void config_request(uint32_t config) {
    LOG_INF("config_request: {Thread Name: %s, Thread Prio: %d}",
            k_thread_name_get(k_current_get()),
            k_thread_priority_get(k_current_get()));
    LOG_INF("Configuration requested: %" PRIu32, config);
}

static void config_response_enabled(bool enabled) {
    LOG_INF("config_response_enabled: {Thread Name: %s, Thread Prio: %d}",
            k_thread_name_get(k_current_get()),
            k_thread_priority_get(k_current_get()));
    LOG_INF("Configuration response: %s", enabled ? "on" : "off");
}

static void connection_param_update(uint16_t interval, uint16_t latency,
                                    uint16_t timeout) {
    LOG_INF("config_param_updated: {Thread Name: %s, Thread Prio: %d}",
            k_thread_name_get(k_current_get()),
            k_thread_priority_get(k_current_get()));
    LOG_INF("Interval: %" PRIu16 ", Latency: %" PRIu16 ", Timeout: %" PRIu16,
            interval, latency, timeout);
}

static void neighbor_state_enabled(bool enabled) {
    LOG_INF("neighbor_state_enabled: {Thread Name: %s, Thread Prio: %d}",
            k_thread_name_get(k_current_get()),
            k_thread_priority_get(k_current_get()));
    LOG_INF("Neighbor state: %s", enabled ? "on" : "off");
}

static void phy_update(enum le_phy phy) {
    LOG_INF("phy_update: {Thread Name: %s, Thread Prio: %d}",
            k_thread_name_get(k_current_get()),
            k_thread_priority_get(k_current_get()));

    switch (phy) {
    case LE_PHY_1M: {
        LOG_INF("1M PHY");
        break;
    }
    case LE_PHY_2M: {
        LOG_INF("2M PHY");
        break;
    }
    case LE_PHY_CODED_S2: {
        LOG_INF("Coded S2 PHY");
        break;
    }
    case LE_PHY_CODED_S8: {
        LOG_INF("Coded S8 PHY");
        break;
    }
    }
}

static void send_config_resp_err(uint8_t err) {
    LOG_INF("send_config_resp_err: {Thread Name: %s, Thread Prio: %d}",
            k_thread_name_get(k_current_get()),
            k_thread_priority_get(k_current_get()));

    LOG_ERR("Send error: %d", err);
}

static void start(uint32_t delay) {
    LOG_INF("start: {Thread Name: %s, Thread Prio: %d}",
            k_thread_name_get(k_current_get()),
            k_thread_priority_get(k_current_get()));

    LOG_INF("Start dealy: %" PRIu32, delay);
}

static int initialize_ble(void) {
    struct ares_ble_init_data init_data = {
        .cb =
            {
                .config_update = config_update,
                .description_update = description_update,
                .config_request = config_request,
                .config_response_enabled = config_response_enabled,
                .connected = connected,
                .connection_param_updated = connection_param_update,
                .disconnected = disconnected,
                .mtu_size_changed = mtu_changed,
                .neighbor_state_enabled = neighbor_state_enabled,
                .phy_updated = phy_update,
                .send_config_response_error = send_config_resp_err,
                .start = start,
            },

        .node_id = 69,
    };

    return ares_init_ble(&init_data);
}
SYS_INIT(initialize_ble, APPLICATION, 90);

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

static int cmd_send_config_response(const struct shell *sh, size_t argc,
                                    char **argv, void *data) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    int err = 0;

    uint32_t config = (uint32_t)data;

    switch (config) {
    case ARES_CONFIG_BANDWIDTH:
    case ARES_CONFIG_CENTER_FREQ:
    case ARES_CONFIG_REF_LEVEL: {
        err = ares_send_config_response(config, &u64_configs[config],
                                        sizeof(u64_configs[config]));
        break;
    }
    case ARES_CONFIG_DURATION: {
        err = ares_send_config_response(
            config, &u32_configs[config - ARES_CONFIG_DURATION],
            sizeof(u32_configs[config - ARES_CONFIG_DURATION]));
        break;
    }
    case ARES_CONFIG_DESCRIPTION: {
        err = ares_send_config_response(
            config, str_configs[config - ARES_CONFIG_DESCRIPTION].config,
            str_configs[config - ARES_CONFIG_DESCRIPTION].len);
        break;
    }
    default: {
        shell_error(sh, "Bad configuration");
        return 1;
    }
    }

    shell_print(sh, "Send result: %d", err);
    return 0;
}

SHELL_SUBCMD_DICT_SET_CREATE(sub_send_config, cmd_send_config_response,
                             (bw, ARES_CONFIG_BANDWIDTH, "Bandwidth"),
                             (cf, ARES_CONFIG_CENTER_FREQ, "Center Frequency"),
                             (ref_lvl, ARES_CONFIG_REF_LEVEL,
                              "Reference level"),
                             (duration, ARES_CONFIG_DURATION, "Duration"),
                             (description, ARES_CONFIG_DESCRIPTION,
                              "Description"));

#define BLE_CONFIG_RESP_HELP "Send a configuration response"

static int cmd_send_neighbor_list(const struct shell *sh, size_t argc,
                                  char **argv) {
    uint8_t test_data[] = {0x11, 0x22, 0x00, 0x33, 0x44,
                           0x01, 0x55, 0x66, 0x01};

    int ret = ares_send_neighbor_states(3, test_data, ARRAY_SIZE(test_data));
    shell_print(sh, "Send neighbors: %d", ret);
    return 0;
}

#define BLE_SEND_NEIGHBOR_HELP "Send a neighbor test message"

static int cmd_disconnect(const struct shell *sh, size_t argc, char **argv) {
    int ret = ares_disconnect_ble();
    shell_print(sh, "Diconnect: %d", ret);
    return 0;
}

#define BLE_DISCONNECT_HELP "Terminate BLE connection"

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_ble, SHELL_CMD(state, &sub_state, BLE_STATE_HELP, NULL),
    SHELL_CMD(config_resp, &sub_send_config, BLE_CONFIG_RESP_HELP, NULL),
    SHELL_CMD(send_neighbors, NULL, BLE_SEND_NEIGHBOR_HELP,
              cmd_send_neighbor_list),
    SHELL_CMD(disconnect, NULL, BLE_DISCONNECT_HELP, cmd_disconnect),
    SHELL_SUBCMD_SET_END, );

SHELL_CMD_REGISTER(ble, &sub_ble, "BLE commands", NULL);
