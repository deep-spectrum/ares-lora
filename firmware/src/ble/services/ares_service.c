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

static bool ready = false;
static uint64_t num_chunks = 0;
static struct ares_ble_service_cb ares_service_cb;

static ssize_t read_ready(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset) {
    const bool *value = attr->user_data;

    LOG_DBG("Attribute read, handle: %u, conn: %p", attr->handle, (void *)conn);

    if (ares_service_cb.read_ready_cb != NULL) {
        ready = ares_service_cb.read_ready_cb();
        return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
                                 sizeof(*value));
    }

    return 0;
}

static ssize_t read_num_chunks(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, void *buf,
                               uint16_t len, uint16_t offset) {
    const uint64_t *value = attr->user_data;

    LOG_DBG("Attribute read, handle: %u, conn: %p", attr->handle, (void *)conn);

    if (ares_service_cb.read_num_chunks_cb != NULL) {
        num_chunks = ares_service_cb.read_num_chunks_cb();
        return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
                                 sizeof(*value));
    }

    return 0;
}

BT_GATT_SERVICE_DEFINE(
    ares_srv_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_ARES_SRV),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_READY, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_ready, NULL, &ready),
    BT_GATT_CHARACTERISTIC(BT_UUID_ARES_SRV_CHUNKS, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_num_chunks, NULL,
                           &num_chunks));
