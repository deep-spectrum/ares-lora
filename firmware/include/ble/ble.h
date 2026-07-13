/**
 * @file ble.h
 *
 * @brief
 *
 * @date 7/9/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_H
#define ARES_BLE_H

#include <stdint.h>
#include <zephyr/kernel.h>

/**
 * @struct ares_ble_callbacks
 * @brief Callbacks for the ble module.
 */
struct ares_ble_callbacks {
    /**
     * Indication that BLE is connected.
     */
    void (*connected)(void);

    /**
     * Indication that BLE is disconnected.
     */
    void (*disconnected)(void);

    /**
     * Indication that the mtu size has changed.
     *
     * @param[in] new_mtu The new mtu size.
     */
    void (*mtu_size_changed)(size_t new_mtu);

    /**
     * Indication that the chunks attribute was subscribed/unsubscribed to.
     *
     * @param[in] enabled `true` if subscribed to, `false` if unsubscribed from.
     */
    void (*chunks_enabled)(bool enabled);

    /**
     * Indication that the image attribute was subscribed/unsubscribed to.
     *
     * @param[in] enabled `true` if subscribed to, `false` if unsubscribed from.
     */
    void (*image_enabled)(bool enabled);
};

/**
 * @struct ares_ble_init_data
 * @brief BLE module initialization information.
 */
struct ares_ble_init_data {
    /**
     * The node ID.
     */
    uint32_t node_id;

    /**
     * Callbacks for the BLE module.
     */
    struct ares_ble_callbacks cb;
};

/**
 * Initialize the BLE module.
 *
 * @param[in] init_data The initialization data for ares ble.
 *
 * @return @p 0 on success.
 * @return @p -EINVAL if @p init_data is @p NULL.
 */
int ares_init_ble(const struct ares_ble_init_data *init_data);

/**
 * Start BLE advertising.
 *
 * @return @p 0 on success.
 */
int ares_enable_ble(void);

/**
 * Stop BLE advertising and terminates any connections.
 *
 * @return @p 0 on success.
 */
int ares_disable_ble(void);

/**
 * Check if BLE is enabled.
 * @return `true` if BLE is active, `false` otherwise.
 */
bool ares_ble_enabled(void);

/**
 * Disconnect BLE and start advertising.
 *
 * @return @p 0 on success.
 */
int ares_disconnect_ble(void);

/**
 * Change the BLE device name.
 *
 * @param[in] node_id The new node ID.
 *
 * @return 0 on success.
 */
int ares_set_ble_node(uint32_t node_id);

/**
 * Tell the central device how many chunks are about to be transferred.
 *
 * @param[in] chunks The number of chunks that need to be transferred.
 *
 * @return 0 on success.
 */
int ares_ble_indicate_chunks(uint64_t chunks);

/**
 * Send a chunk to the central device.
 *
 * @param[in] chunk Pointer to the first byte in the chunk.
 * @param[in] num_bytes The number of bytes in the chunk.
 *
 * @return 0 on success.
 */
int ares_ble_send_chunk(const uint8_t *chunk, size_t num_bytes);

#endif // ARES_BLE_H
