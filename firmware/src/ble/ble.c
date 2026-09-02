/**
 * @file ble.c
 *
 * @brief
 *
 * @date 7/9/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ble/ble.h>
#include <ble/services/ares_service.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_app);

#define NAME_SD_IDX 0

enum {
    BLE_INITIALIZED,
    BLE_ACTIVE,
    BLE_ADVERTISING,
    BLE_CONNECTED,
};

enum {
    BLE_SIGNAL_CONFIG_RESP_IND,

    BLE_SIGNAL_LAST,
};

struct ble_conn_info {
    struct k_poll_signal signals[BLE_SIGNAL_LAST];
    struct k_poll_event events[BLE_SIGNAL_LAST];
    struct k_sem adv_name_sem;

    atomic_t state;
    size_t payload_mtu_size;
    struct bt_conn *conn;
};

static char adv_name[16] = "Ares";

static const struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
    (BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY), 800, 801, NULL);

static struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_ARES_SRV_VAL),
};

static struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, adv_name, 4),
};

static struct ble_conn_info connection_info;
static struct ares_ble_callbacks callbacks;

static void adv_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    int err;

    if (!atomic_test_bit(&connection_info.state, BLE_ACTIVE)) {
        return;
    }

    k_sem_take(&connection_info.adv_name_sem, K_FOREVER);

    err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

    if (err != 0) {
        LOG_ERR("Failed to start advertising: %d", err);
        k_sem_give(&connection_info.adv_name_sem);
        return;
    }

    LOG_DBG("Advertising started");
    atomic_set_bit(&connection_info.state, BLE_ADVERTISING);
    k_sem_give(&connection_info.adv_name_sem);
}
K_WORK_DEFINE(adv_work, adv_work_handler);

static void advertising_start(void) { k_work_submit(&adv_work); }

static void recycled_cb(void) { advertising_start(); }

static void exchange_mtu_cb(struct bt_conn *conn, uint8_t att_err,
                            struct bt_gatt_exchange_params *params) {
    ARG_UNUSED(params);
    LOG_INF("MTU exchange %s", att_err == 0 ? "successful" : "failed");
    if (att_err == 0) {
        connection_info.payload_mtu_size = bt_gatt_get_mtu(conn) - 3;
        if (callbacks.mtu_size_changed != NULL) {
            callbacks.mtu_size_changed(connection_info.payload_mtu_size);
        }
    }
}

static void update_mtu(struct bt_conn *conn) {
    static struct bt_gatt_exchange_params params = {.func = exchange_mtu_cb};
    int err = bt_gatt_exchange_mtu(conn, &params);
    if (err != 0) {
        LOG_ERR("bt_gatt_exchange_mtu(): %d", err);
    }
}

static void on_connected(struct bt_conn *conn, uint8_t bt_err) {
    int err;
    struct bt_conn_info info;

    if (bt_err != 0) {
        LOG_ERR("Connection error (%d)", bt_err);
        return;
    }

    connection_info.conn = bt_conn_ref(conn);
    atomic_set_bit(&connection_info.state, BLE_CONNECTED);
    atomic_clear_bit(&connection_info.state, BLE_ADVERTISING);

    err = bt_conn_get_info(conn, &info);
    if (err != 0) {
        LOG_ERR("bt_conn_get_info(): %d", err);
        return;
    }

    if (callbacks.connected != NULL) {
        callbacks.connected();
    }

    update_mtu(conn);
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason) {
    ARG_UNUSED(conn);
    ARG_UNUSED(reason);
    LOG_INF("Diconnected (%d: %s)", reason, bt_att_err_to_str(reason));
    bt_conn_unref(connection_info.conn);
    connection_info.conn = NULL;
    atomic_clear_bit(&connection_info.state, BLE_CONNECTED);

    if (callbacks.disconnected != NULL) {
        callbacks.disconnected();
    }
}

BT_CONN_CB_DEFINE(conn_cb) = {
    .connected = on_connected,
    .disconnected = on_disconnected,
    .recycled = recycled_cb,
};

static void config_response_indicate_callback(struct bt_conn *conn,
                                              uint8_t err) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);

    k_poll_signal_raise(&connection_info.signals[BLE_SIGNAL_CONFIG_RESP_IND],
                        err);
}

static void bandwidth_update(struct bt_conn *conn, uint64_t bandwidth) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);

    callbacks.config_update(ARES_CONFIG_BANDWIDTH, bandwidth);
}

static void center_frequency_update(struct bt_conn *conn,
                                    uint64_t center_freq) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);

    callbacks.config_update(ARES_CONFIG_CENTER_FREQ, center_freq);
}

static void reference_level_update(struct bt_conn *conn, uint64_t ref_level) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);

    callbacks.config_update(ARES_CONFIG_REF_LEVEL, ref_level);
}

static void duration_update(struct bt_conn *conn, uint32_t duration) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);
    uint64_t val = 0;
    val = duration;

    callbacks.config_update(ARES_CONFIG_DURATION, val);
}

static void description_update(struct bt_conn *conn, const void *buf,
                               uint16_t len) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);

    // TODO
}

static void config_read_handler(struct bt_conn *conn,
                                enum ares_srv_configs config) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);

    callbacks.config_request(config);
}

static void start_handler(struct bt_conn *conn, uint32_t delay) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);

    callbacks.start(delay);
}

int ares_init_ble(const struct ares_ble_init_data *init_data) {
    struct ares_service_cb service_cb = {
        .bandwidth_update = bandwidth_update,
        .center_frequency_update = center_frequency_update,
        .reference_level_update = reference_level_update,
        .duration_update = duration_update,
        .description_update = description_update,
        .config_read = config_read_handler,
        .config_response_ind_cb = config_response_indicate_callback,
        .start = start_handler,
    };

    int err;

    if (init_data == NULL) {
        return -EINVAL;
    }

    if (atomic_test_bit(&connection_info.state, BLE_INITIALIZED)) {
        return -EALREADY;
    }

    callbacks = init_data->cb;

    for (size_t i = 0; i < BLE_SIGNAL_LAST; i++) {
        k_poll_signal_init(&connection_info.signals[i]);
        k_poll_event_init(&connection_info.events[i], K_POLL_TYPE_SIGNAL,
                          K_POLL_MODE_NOTIFY_ONLY, &connection_info.signals[i]);
    }

    k_sem_init(&connection_info.adv_name_sem, 1, 1);

    service_cb.config_response_ind_enabled = callbacks.config_response_enabled;
    service_cb.neighbor_state_enabled = callbacks.neighbor_state_enabled;

    bt_ares_srv_init(&service_cb);

    atomic_set_bit(&connection_info.state, BLE_INITIALIZED);
    err = ares_set_ble_node(init_data->node_id);
    if (err != 0) {
        return err;
    }

    return bt_enable(NULL);
}

int ares_enable_ble(void) {
    if (!atomic_test_bit(&connection_info.state, BLE_INITIALIZED)) {
        return -ECANCELED;
    }

    atomic_set_bit(&connection_info.state, BLE_ACTIVE);

    advertising_start();

    return 0;
}

int ares_disable_ble(void) {
    int ret = -ECANCELED;

    if (!atomic_test_bit(&connection_info.state, BLE_INITIALIZED)) {
        return ret;
    }

    atomic_clear_bit(&connection_info.state, BLE_ACTIVE);

    if (atomic_test_bit(&connection_info.state, BLE_CONNECTED)) {
        ret = ares_disconnect_ble();
    } else if (atomic_test_bit(&connection_info.state, BLE_ADVERTISING)) {
        ret = bt_le_adv_stop();
        if (ret != 0) {
            LOG_ERR("bt_le_adv_stop(): %d", ret);
        }
        atomic_clear_bit(&connection_info.state, BLE_ADVERTISING);
    }

    return ret;
}

bool ares_ble_enabled(void) {
    return atomic_test_bit(&connection_info.state, BLE_ACTIVE);
}

int ares_disconnect_ble(void) {
    if (!atomic_test_bit(&connection_info.state, BLE_CONNECTED)) {
        return -EALREADY;
    }

    return bt_conn_disconnect(connection_info.conn,
                              BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

int ares_set_ble_node(uint32_t node_id) {
    size_t len;
    struct bt_data name_data = {.type = BT_DATA_NAME_COMPLETE,
                                .data = (const uint8_t *)adv_name};

    k_sem_take(&connection_info.adv_name_sem, K_FOREVER);

    if (atomic_test_bit(&connection_info.state, BLE_ADVERTISING)) {
        k_sem_give(&connection_info.adv_name_sem);
        return -EBUSY;
    }

    len = snprintk(adv_name, sizeof(adv_name), "Ares %u", node_id - 1);
    name_data.data_len = len;
    sd[NAME_SD_IDX] = name_data;

    k_sem_give(&connection_info.adv_name_sem);

    return 0;
}

#define ARES_BLE_CHECK_MSG_LEN(ret, len, type)                                 \
    do {                                                                       \
        if (len != sizeof(type)) {                                             \
            ret = -EBADMSG;                                                    \
        }                                                                      \
    } while (false)

static int check_response_size(uint32_t type, size_t len) {
    int ret = 0;

    switch (type) {
    case ARES_CONFIG_BANDWIDTH:
    case ARES_CONFIG_CENTER_FREQ:
    case ARES_CONFIG_REF_LEVEL: {
        ARES_BLE_CHECK_MSG_LEN(ret, len, uint64_t);
        break;
    }
    case ARES_CONFIG_DURATION: {
        ARES_BLE_CHECK_MSG_LEN(ret, len, uint32_t);
        break;
    }
    case ARES_CONFIG_DESCRIPTION: {
        // todo: check against a config
        break;
    }
    default: {
        ret = -EINVAL;
        break;
    }
    }

    return ret;
}

int ares_send_config_response(uint32_t type, const void *config, size_t len) {
    int ret;
    unsigned int signaled;

    if (!atomic_test_bit(&connection_info.state, BLE_INITIALIZED)) {
        return -ECANCELED;
    }

    ret = check_response_size(type, len);
    if (ret < 0) {
        return ret;
    }

    // todo: allocate buffer & copy data
    // todo: send data

    k_poll(&connection_info.events[BLE_SIGNAL_CONFIG_RESP_IND], 1, K_FOREVER);
    k_poll_signal_check(&connection_info.signals[BLE_SIGNAL_CONFIG_RESP_IND],
                        &signaled, &ret);
    k_poll_signal_reset(&connection_info.signals[BLE_SIGNAL_CONFIG_RESP_IND]);
    connection_info.events[BLE_SIGNAL_CONFIG_RESP_IND].state =
        K_POLL_STATE_NOT_READY;

    __ASSERT_NO_MSG(signaled);
    // todo: deallocate buffer

    return ret;
}

int ares_send_neighbor_states(uint8_t num_neighbors, const void *data,
                              size_t len) {
    size_t buf_len;
    if (!atomic_test_bit(&connection_info.state, BLE_INITIALIZED)) {
        return -ECANCELED;
    }

    buf_len = (size_t)num_neighbors * 3;
    if (buf_len != len) {
        return -EBADMSG;
    }

    buf_len += sizeof(num_neighbors);

    // todo: Allocate & copy data
    // todo: send data
    // todo: deallocate

    return 0;
}
