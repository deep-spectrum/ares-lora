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
 * Available LE PHYs.
 */
enum le_phy {
    LE_PHY_1M,       ///< 1Mbps
    LE_PHY_2M,       ///< 2Mbps
    LE_PHY_CODED_S2, ///< Coded PHY S=2
    LE_PHY_CODED_S8, ///< Coded PHY S=8
};

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
     * Indication that the connection parameters have been changed.
     *
     * @param[in] interval The connection interval.
     * @param[in] latency The connection latency.
     * @param[in] timeout The supervisor timeout.
     */
    void (*connection_param_updated)(uint16_t interval, uint16_t latency,
                                     uint16_t timeout);

    /**
     * Indication that the PHY of the connection has changed.
     *
     * @param[in] phy The new PHY of the connection.
     */
    void (*phy_updated)(enum le_phy phy);

    /**
     * Indication that the config response attribute was subscribed/unsubscribed
     * to.
     *
     * @param[in] enabled `true` if subscribed to, `false` if unsubscribed from.
     */
    void (*config_response_enabled)(bool enabled);

    /**
     * Indication that the neighbor state attribute was subscribed/unsubscribed
     * to.
     *
     * @param[in] enabled `true` if subscribed to, `false` if unsubscribed from.
     */
    void (*neighbor_state_enabled)(bool enabled);

    /**
     * Notification for a configuration change.
     *
     * @param[in] type The configuration type.
     * @param[in] value The new value of the configuration.
     */
    void (*config_update)(uint32_t type, uint64_t value);

    /**
     * Notification that the description changed.
     *
     * @param[in] buf The buffer that stores the new description.
     * @param[in] len The length of the buffer.
     */
    void (*description_update)(const uint8_t *buf, size_t len);

    /**
     * Notification that the central device is requesting a configuration.
     *
     * @param[in] type The configuration being requested.
     */
    void (*config_request)(uint32_t type);

    /**
     * Notification that the run is ready to be started.
     *
     * @param[in] start_delay The amount of seconds to schedule out the start
     * time by.
     */
    void (*start)(uint32_t start_delay);
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
 * Send a response to a configuration read request.
 *
 * @param[in] type The configuration type.
 * @param[in] config The config data.
 * @param[in] len The length of the config data.
 *
 * @return 0 on success.
 */
int ares_send_config_response(uint32_t type, const void *config, size_t len);

/**
 * Send neighbor state information over BLE.
 *
 * @param[in] num_neighbors The number of neighbors.
 * @param[in] data The neighbor information.
 * @param[in] len The length of the neighbor information.
 *
 * @return 0 on success.
 */
int ares_send_neighbor_states(uint8_t num_neighbors, const void *data,
                              size_t len);

#endif // ARES_BLE_H
