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
    BLE_SIGNAL_CHUNK_IND,
    BLE_SIGNAL_IMAGE_IND,

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

static void chunks_indicate_callback(struct bt_conn *conn, uint8_t err) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);

    k_poll_signal_raise(&connection_info.signals[BLE_SIGNAL_CHUNK_IND], err);
}

static void image_indicate_callback(struct bt_conn *conn, uint8_t err) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED) &&
                    atomic_test_bit(connection_info.state, BLE_CONNECTED));
    ARG_UNUSED(conn);

    k_poll_signal_raise(&connection_info.signals[BLE_SIGNAL_IMAGE_IND], err);
}

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

int ares_init_ble(const struct ares_ble_init_data *init_data) {
    struct ares_service_cb service_cb = {
        .num_chunks_ind_cb = chunks_indicate_callback,
        .image_ind_cb = image_indicate_callback,
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

    service_cb.num_chunks_ind_enabled = callbacks.chunks_enabled;
    service_cb.image_ind_enabled = callbacks.image_enabled;

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

int ares_ble_indicate_chunks(uint64_t chunks) {
    int ret;
    unsigned int signaled;

    if (!atomic_test_bit(&connection_info.state, BLE_INITIALIZED)) {
        return -ECANCELED;
    }

    ret = bt_ares_srv_ind_chunks(chunks);
    if (ret != 0) {
        return ret;
    }

    k_poll(&connection_info.events[BLE_SIGNAL_CHUNK_IND], 1, K_FOREVER);
    k_poll_signal_check(&connection_info.signals[BLE_SIGNAL_CHUNK_IND],
                        &signaled, &ret);

    __ASSERT_NO_MSG(signaled);

    return ret;
}

int ares_ble_send_chunk(const uint8_t *chunk, size_t num_bytes) {
    int ret;
    unsigned int signaled;

    if (!atomic_test_bit(&connection_info.state, BLE_INITIALIZED)) {
        return -ECANCELED;
    }

    if (num_bytes > connection_info.payload_mtu_size) {
        return -ENOBUFS;
    }

    ret = bt_ares_srv_ind_image_chunk(chunk, num_bytes);
    if (ret != 0) {
        return ret;
    }

    k_poll(&connection_info.events[BLE_SIGNAL_IMAGE_IND], 1, K_FOREVER);
    k_poll_signal_check(&connection_info.signals[BLE_SIGNAL_IMAGE_IND],
                        &signaled, &ret);

    __ASSERT_NO_MSG(signaled);

    return ret;
}
