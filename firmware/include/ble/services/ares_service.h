/**
 * @file ares_service.h
 *
 * @brief
 *
 * @date 7/9/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_ARES_SERVICE_H
#define ARES_ARES_SERVICE_H

#include <zephyr/bluetooth/uuid.h>

/**
 * @brief Ares service UUID.
 *
 * f2765f1d-d570-48cf-a6b7-985ff6af492c
 */
#define BT_UUID_ARES_SRV_VAL                                                   \
    BT_UUID_128_ENCODE(0xf2765f1d, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

/**
 * @brief Ares number of chunks UUID.
 */
#define BT_UUID_ARES_SRV_CHUNKS_VAL                                            \
    BT_UUID_128_ENCODE(0xf2765f1e, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

/**
 * @brief Ares image UUID.
 */
#define BT_UUID_ARES_SRV_IMAGE_VAL                                             \
    BT_UUID_128_ENCODE(0xf2765f1f, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

#define BT_UUID_ARES_SRV        BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_VAL)
#define BT_UUID_ARES_SRV_CHUNKS BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_CHUNKS_VAL)
#define BT_UUID_ARES_SRV_IMAGE  BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_IMAGE_VAL)

struct ares_ble_service_cb {
    void (*num_chunks_ind_enabled)(bool enabled);
    void (*image_ind_enabled)(bool enabled);

    void (*num_chunks_ind_cb)(const struct bt_conn *conn, uint8_t err);
    void (*image_ind_cb)(const struct bt_conn *conn, uint8_t err);
};

int bt_ares_srv_init(const struct ares_ble_service_cb *cb);

int bt_ares_srv_ind_chunks(uint64_t chunks);

int bt_ares_srv_ind_image_chunk(const uint8_t *bytes, size_t num_bytes);

#endif // ARES_ARES_SERVICE_H
