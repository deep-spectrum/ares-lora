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
    CHUNKS_ENABLED,
    IMAGE_ENABLED,
};

static struct ares_ble_service_cb ares_service_cb;
static atomic_t state;

static void ares_service_chunk_cfg_changed(const struct bt_gatt_attr *attr,
                                           uint16_t value) {
    ARG_UNUSED(attr);
    bool enabled = value == BT_GATT_CCC_INDICATE;

    LOG_DBG("Indication for chunks has been turned %s", enabled ? "on" : "off");

    if (ares_service_cb.num_chunks_ind_enabled != NULL) {
        ares_service_cb.num_chunks_ind_enabled(enabled);
    }

    if (enabled) {
        atomic_set_bit(&state, CHUNKS_ENABLED);
    } else {
        atomic_clear_bit(&state, CHUNKS_ENABLED);
    }
}

static void ares_service_image_cfg_changed(const struct bt_gatt_attr *attr,
                                           uint16_t value) {
    ARG_UNUSED(attr);
    bool enabled = value == BT_GATT_CCC_INDICATE;

    LOG_DBG("Indication for image has been turned %s", enabled ? "on" : "off");

    if (ares_service_cb.image_ind_enabled != NULL) {
        ares_service_cb.image_ind_enabled(enabled);
    }

    if (enabled) {
        atomic_set_bit(&state, IMAGE_ENABLED);
    } else {
        atomic_clear_bit(&state, IMAGE_ENABLED);
    }
}

BT_GATT_SERVICE_DEFINE(
    ares_srv_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_ARES_SRV),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_CHUNKS, BT_GATT_CHRC_INDICATE,
                           BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(ares_service_chunk_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_IMAGE, BT_GATT_CHRC_INDICATE,
                           BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(ares_service_image_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

int bt_ares_srv_init(const struct ares_ble_service_cb *cb) {
    if (cb == NULL) {
        return -EINVAL;
    }

    ares_service_cb.num_chunks_ind_enabled = cb->num_chunks_ind_enabled;
    ares_service_cb.image_ind_enabled = cb->image_ind_enabled;

    return 0;
}
