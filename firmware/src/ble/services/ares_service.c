/**
 * @file ares_service.c
 *
 * @brief
 *
 * @date 7/9/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <ble/services/ares_service.h>

LOG_MODULE_REGISTER(ares_ble_service);

enum {
    ARES_CONFIG_RESP_ENABLED,
    ARES_NEIGHBOR_STATE_ENABLED,
};

struct ares_srv_ctx {
    struct ares_service_cb ares_service_cb;
    atomic_t state;
};

static struct ares_srv_ctx srv_ctx;

static void
ares_service_config_resp_cfg_changed(const struct bt_gatt_attr *attr,
                                     uint16_t value) {
    ARG_UNUSED(attr);
    bool enabled = value == BT_GATT_CCC_INDICATE;

    LOG_DBG("Indication for config response has been turned %s",
            enabled ? "on" : "off");

    if (srv_ctx.ares_service_cb.config_response_ind_enabled != NULL) {
        srv_ctx.ares_service_cb.config_response_ind_enabled(enabled);
    }

    if (enabled) {
        atomic_set_bit(&srv_ctx.state, ARES_CONFIG_RESP_ENABLED);
    } else {
        atomic_clear_bit(&srv_ctx.state, ARES_CONFIG_RESP_ENABLED);
    }
}

static void
ares_service_neighbor_update_cfg_changed(const struct bt_gatt_attr *attr,
                                         uint16_t value) {
    ARG_UNUSED(attr);
    bool enabled = value == BT_GATT_CCC_NOTIFY;

    LOG_DBG("Notification for neighbor updates has been turned %s",
            enabled ? "on" : "off");

    if (srv_ctx.ares_service_cb.neighbor_state_enabled != NULL) {
        srv_ctx.ares_service_cb.neighbor_state_enabled(enabled);
    }

    if (enabled) {
        atomic_set_bit(&srv_ctx.state, ARES_NEIGHBOR_STATE_ENABLED);
    } else {
        atomic_clear_bit(&srv_ctx.state, ARES_NEIGHBOR_STATE_ENABLED);
    }
}

static ssize_t write_config_common(const struct bt_gatt_attr *attr,
                                   uint16_t len, uint16_t offset,
                                   uint16_t type_size) {
    ARG_UNUSED(attr);
    LOG_DBG("Attribute write, handle: %u, conn %p", attr->handle, conn);

    if (len != type_size) {
        LOG_DBG("write bandwidth: Incorecct data length");
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (offset != 0u) {
        LOG_DBG("write bandwidth: Incorrect data offset");
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
}

static ssize_t write_bandwidth(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
    ARG_UNUSED(flags);
    struct ares_srv_ctx *ctx = attr->user_data;
    ssize_t ret = write_config_common(attr, len, offset, sizeof(uint64_t));

    if (ctx->ares_service_cb.bandwidth_update != NULL &&
        ret == BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED)) {
        uint64_t bw = *((uint64_t *)buf);
        ctx->ares_service_cb.bandwidth_update(conn, bw);
        ret = len;
    }

    return ret;
}

static ssize_t write_center_frequency(struct bt_conn *conn,
                                      const struct bt_gatt_attr *attr,
                                      const void *buf, uint16_t len,
                                      uint16_t offset, uint8_t flags) {
    ARG_UNUSED(flags);
    struct ares_srv_ctx *ctx = attr->user_data;
    ssize_t ret = write_config_common(attr, len, offset, sizeof(uint64_t));

    if (ctx->ares_service_cb.center_frequency_update != NULL &&
        ret == BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED)) {
        uint64_t freq = *((uint64_t *)buf);
        ctx->ares_service_cb.center_frequency_update(conn, freq);
        ret = len;
    }

    return ret;
}

static ssize_t write_ref_level(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
    ARG_UNUSED(flags);
    struct ares_srv_ctx *ctx = attr->user_data;
    ssize_t ret = write_config_common(attr, len, offset, sizeof(uint64_t));

    if (ctx->ares_service_cb.reference_level_update != NULL &&
        ret == BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED)) {
        uint64_t ref_level = *((uint64_t *)buf);
        ctx->ares_service_cb.reference_level_update(conn, ref_level);
        ret = len;
    }

    return ret;
}

static ssize_t write_duration(struct bt_conn *conn,
                              const struct bt_gatt_attr *attr, const void *buf,
                              uint16_t len, uint16_t offset, uint8_t flags) {
    ARG_UNUSED(flags);
    struct ares_srv_ctx *ctx = attr->user_data;
    ssize_t ret = write_config_common(attr, len, offset, sizeof(uint32_t));

    if (ctx->ares_service_cb.duration_update != NULL &&
        ret == BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED)) {
        uint32_t duration = *((uint32_t *)buf);
        ctx->ares_service_cb.duration_update(conn, duration);
        ret = len;
    }

    return ret;
}

static ssize_t write_description(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len, uint16_t offset,
                                 uint8_t flags) {
    ARG_UNUSED(flags);
    ARG_UNUSED(offset);

    struct ares_srv_ctx *ctx = attr->user_data;
    ssize_t ret = BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);

    if (ctx->ares_service_cb.description_update != NULL) {
        ctx->ares_service_cb.description_update(conn, buf, len);
        ret = len;
    }

    return ret;
}

static ssize_t write_config_read(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len, uint16_t offset,
                                 uint8_t flags) {
    ARG_UNUSED(flags);
    struct ares_srv_ctx *ctx = attr->user_data;
    ssize_t ret =
        write_config_common(attr, len, offset, sizeof(enum ares_srv_configs));

    if (ctx->ares_service_cb.config_read != NULL &&
        ret == BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED)) {
        enum ares_srv_configs config = *((enum ares_srv_configs *)buf);

        if (config < ARES_CONFIG_INVALID) {
            ctx->ares_service_cb.config_read(conn, config);
            ret = len;
        } else {
            ret = BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
        }
    }

    return ret;
}

static ssize_t write_start(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr, const void *buf,
                           uint16_t len, uint16_t offset, uint8_t flags) {
    ARG_UNUSED(flags);
    struct ares_srv_ctx *ctx = attr->user_data;
    ssize_t ret = write_config_common(attr, len, offset, sizeof(uint32_t));

    if (ctx->ares_service_cb.start != NULL && ret == BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED)) {
        uint32_t delay = *((uint32_t *)buf);
        ctx->ares_service_cb.start(conn, delay);
        ret = len;
    }

    return ret;
}

BT_GATT_SERVICE_DEFINE(
    ares_srv_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_ARES_SRV),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_BANDWIDTH, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, write_bandwidth, &srv_ctx),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_CENTER_FREQ, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, write_center_frequency,
                           &srv_ctx),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_REF_LEVEL, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, write_ref_level, &srv_ctx),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_DURATION, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, write_duration, &srv_ctx),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_DESCRIPTION, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, write_description,
                           &srv_ctx),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_CONFIG_READ, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, write_config_read,
                           &srv_ctx),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_CONFIG_RESP, BT_GATT_CHRC_INDICATE,
                           BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(ares_service_config_resp_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_START, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, write_start, &srv_ctx),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_NEIGHBOR_STATE, BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(ares_service_neighbor_update_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

int bt_ares_srv_init(const struct ares_service_cb *cb) {
    if (cb == NULL) {
        return -EINVAL;
    }

    srv_ctx.ares_service_cb = *cb;

    return 0;
}

static void config_response_ind_cb(struct bt_conn *conn,
                                   struct bt_gatt_indicate_params *params,
                                   uint8_t err) {
    ARG_UNUSED(params);

    LOG_DBG("Indication %s\n", err != 0U ? "fail" : "success");

    if (srv_ctx.ares_service_cb.config_response_ind_cb != NULL) {
        srv_ctx.ares_service_cb.config_response_ind_cb(conn, err);
    }
}

int bt_ares_config_response(struct bt_conn *conn, const void *data,
                            size_t len) {
    static struct bt_gatt_indicate_params ind_params = {
        .func = config_response_ind_cb,
    };

    if (!atomic_test_bit(&srv_ctx.state, ARES_CONFIG_RESP_ENABLED)) {
        return -EACCES;
    }

    if (data == NULL) {
        return -EINVAL;
    }

    ind_params.attr = &ares_srv_svc.attrs[16];
    ind_params.data = data;
    ind_params.len = len;
    return bt_gatt_indicate(conn, &ind_params);
}

int bt_ares_notify_neighbor_state(struct bt_conn *conn, const void *data, size_t len) {
    if (!atomic_test_bit(&srv_ctx.state, ARES_NEIGHBOR_STATE_ENABLED)) {
        return -EACCES;
    }

    if (data == NULL) {
        return -EINVAL;
    }

    return bt_gatt_notify(conn, &ares_srv_svc.attrs[21], data, len);
}
