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
#include <zephyr/net_buf.h>

LOG_MODULE_REGISTER(ble_app, CONFIG_BLE_APP_LOG_LEVEL);

#define NAME_SD_IDX 0

enum {
    BLE_INITIALIZED,
    BLE_ACTIVE,
    BLE_ADVERTISING,
    BLE_CONNECTED,
};

// todo
#define CONFIG_ARES_BLE_WORKQ_PRIO       1
#define CONFIG_ARES_BLE_WORKQ_STACK_SIZE 1024

K_THREAD_STACK_DEFINE(ble_workq_stack, CONFIG_ARES_BLE_WORKQ_STACK_SIZE);

// todo
#define CONFIG_ARES_BLE_NUM_NET_BUFS 4
#define CONFIG_ARES_BLE_NETBUF_SIZE  1536

NET_BUF_POOL_DEFINE(ares_tx_netbuf, CONFIG_ARES_BLE_NUM_NET_BUFS,
                    CONFIG_ARES_BLE_NETBUF_SIZE, 0, NULL);
NET_BUF_POOL_DEFINE(ares_rx_netbuf, CONFIG_ARES_BLE_NUM_NET_BUFS,
                    CONFIG_ARES_BLE_NETBUF_SIZE, 0, NULL);

struct config_response_ind_err_work {
    uint8_t err;
    struct k_work work;
};

struct config_write_work {
    struct k_sem sem;
    uint64_t value;
    const enum ares_srv_configs config;
    struct k_work work;
};

#define Z_ARES_CONFIG_WRITE_WORK_INITIALIZER(name, _config, _handler)          \
    {                                                                          \
        .config = (_config), .work = Z_WORK_INITIALIZER(_handler),             \
        .sem = Z_SEM_INITIALIZER(name.sem, 1, 1)                               \
    }

#define ARES_CONFIG_WRITE_WORK_DEFINE(name, _config, _handler)                 \
    struct config_write_work name =                                            \
        Z_ARES_CONFIG_WRITE_WORK_INITIALIZER(name, _config, _handler)

struct ble_conn_info {
    struct k_sem adv_name_sem;

    atomic_t state;
    size_t payload_mtu_size;
    struct bt_conn *conn;

    struct net_buf *desc_buf;
    struct net_buf *config_resp;

    struct config_response_ind_err_work conf_resp_work;

    struct k_work_q write_work_q;
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

static void config_response_indicate_work(struct k_work *work) {
    struct config_response_ind_err_work *cwork =
        CONTAINER_OF(work, struct config_response_ind_err_work, work);

    if (callbacks.send_config_response_error != NULL) {
        callbacks.send_config_response_error(cwork->err);
    }
}

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

static void config_response_indicate_callback(struct bt_conn *conn, uint8_t err,
                                              struct net_buf *buf) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);

    net_buf_unref(buf);

    if (err != BT_ATT_ERR_SUCCESS) {
        connection_info.conf_resp_work.err = err;
        k_work_submit(&connection_info.conf_resp_work.work);
    }
}

static void write_work_handler(struct k_work *work) {
    struct config_write_work *wwork =
        CONTAINER_OF(work, struct config_write_work, work);
    uint64_t value = wwork->value;
    enum ares_srv_configs config = wwork->config;
    k_sem_give(&wwork->sem);

    if (callbacks.config_update != NULL) {
        callbacks.config_update(config, value);
    }
}

ARES_CONFIG_WRITE_WORK_DEFINE(bandwidth_work, ARES_CONFIG_BANDWIDTH,
                              write_work_handler);
ARES_CONFIG_WRITE_WORK_DEFINE(center_freq_work, ARES_CONFIG_CENTER_FREQ,
                              write_work_handler);
ARES_CONFIG_WRITE_WORK_DEFINE(ref_level_work, ARES_CONFIG_REF_LEVEL,
                              write_work_handler);
ARES_CONFIG_WRITE_WORK_DEFINE(duration_work, ARES_CONFIG_DURATION,
                              write_work_handler);

static enum ares_srv_write_response
submit_write_work(struct config_write_work *work, uint64_t value) {
    int ret = k_sem_take(&work->sem, K_NO_WAIT);
    if (ret < 0) {
        return ARES_WRITE_BUSY;
    }

    work->value = value;
    ret = k_work_submit_to_queue(&connection_info.write_work_q, &work->work);
    if (ret < 0) {
        k_sem_give(&work->sem);
        return ARES_WRITE_FAILED;
    }

    return ARES_WRITE_SUCCESS;
}

static enum ares_srv_write_response bandwidth_update(struct bt_conn *conn,
                                                     uint64_t bandwidth) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);
    LOG_DBG("Bandwidth update thread priority: %d",
            k_thread_priority_get(k_current_get()));

    return submit_write_work(&bandwidth_work, bandwidth);
}

static enum ares_srv_write_response
center_frequency_update(struct bt_conn *conn, uint64_t center_freq) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);
    LOG_DBG("Center frequency update thread priority: %d",
            k_thread_priority_get(k_current_get()));

    return submit_write_work(&center_freq_work, center_freq);
}

static enum ares_srv_write_response reference_level_update(struct bt_conn *conn,
                                                           uint64_t ref_level) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);
    LOG_DBG("Reference level update thread priority: %d",
            k_thread_priority_get(k_current_get()));

    return submit_write_work(&ref_level_work, ref_level);
}

static enum ares_srv_write_response duration_update(struct bt_conn *conn,
                                                    uint32_t duration) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);
    LOG_DBG("duration update thread priority: %d",
            k_thread_priority_get(k_current_get()));
    uint64_t val = 0;
    val = duration;

    return submit_write_work(&duration_work, val);
}

static void description_update(struct bt_conn *conn, const void *buf,
                               uint16_t len) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);
    LOG_DBG("Description update thread priority: %d",
            k_thread_priority_get(k_current_get()));

    if (connection_info.desc_buf == NULL) {
        connection_info.desc_buf = net_buf_alloc(&ares_rx_netbuf, K_NO_WAIT);
        if (connection_info.desc_buf == NULL) {
            return;
        }
    }

    net_buf_add_mem(connection_info.desc_buf, buf, len);
    // TODO: Submit work
}

static void config_read_handler(struct bt_conn *conn,
                                enum ares_srv_configs config) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);
    LOG_DBG("Config read thread priority: %d",
            k_thread_priority_get(k_current_get()));

    callbacks.config_request(config);
}

static void start_handler(struct bt_conn *conn, uint32_t delay) {
    __ASSERT_NO_MSG(conn == connection_info.conn);
    __ASSERT_NO_MSG(atomic_test_bit(connection_info.state, BLE_INITIALIZED));
    ARG_UNUSED(conn);
    LOG_DBG("Start thread priority: %d",
            k_thread_priority_get(k_current_get()));

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
    struct k_work_queue_config workq_config = {
        .essential = true,
        .name = "Ares BLE RX WQ",
    };

    int err;

    if (init_data == NULL) {
        return -EINVAL;
    }

    if (atomic_test_bit(&connection_info.state, BLE_INITIALIZED)) {
        return -EALREADY;
    }

    k_work_queue_init(&connection_info.write_work_q);
    k_work_queue_start(&connection_info.write_work_q, ble_workq_stack,
                       K_THREAD_STACK_SIZEOF(ble_workq_stack),
                       CONFIG_ARES_BLE_WORKQ_PRIO, &workq_config);

    k_work_init(&connection_info.conf_resp_work.work,
                config_response_indicate_work);

    callbacks = init_data->cb;

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
        if (len >= (size_t)CONFIG_ARES_BLE_NETBUF_SIZE) {
            ret = -ENOMEM;
        }
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
    struct net_buf *buffer;
    uint16_t len_ = (uint16_t)len;

    if (!atomic_test_bit(&connection_info.state, BLE_INITIALIZED)) {
        return -ECANCELED;
    }

    ret = check_response_size(type, len);
    if (ret < 0) {
        return ret;
    }

    buffer = net_buf_alloc(&ares_tx_netbuf, K_MSEC(100));
    if (buffer == NULL) {
        return -ENOMEM;
    }

    net_buf_add_mem(buffer, &type, sizeof(type));
    net_buf_add_mem(buffer, &len_, sizeof(len_));
    net_buf_add_mem(buffer, config, len);

    ret = bt_ares_config_response(connection_info.conn, buffer);
    if (ret < 0) {
        net_buf_unref(buffer);
    }

    return ret;
}

int ares_send_neighbor_states(uint8_t num_neighbors, const void *data,
                              size_t len) {
    size_t buf_len;
    struct net_buf *buf;
    int ret;

    if (!atomic_test_bit(&connection_info.state, BLE_INITIALIZED)) {
        return -ECANCELED;
    }

    buf_len = (size_t)num_neighbors * 3;
    if (buf_len != len) {
        return -EBADMSG;
    }

    buf_len += sizeof(num_neighbors);

    buf = net_buf_alloc(&ares_tx_netbuf, K_MSEC(100));
    if (buf == NULL) {
        return -ENOMEM;
    }

    net_buf_add_mem(buf, &buf_len, sizeof(buf_len));
    net_buf_add_mem(buf, data, len);

    ret = bt_ares_notify_neighbor_state(connection_info.conn, buf->data,
                                        buf->len);

    net_buf_unref(buf);
    return ret;
}
