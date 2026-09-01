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

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>

/**
 * @brief Ares service UUID.
 *
 * f2765f1d-d570-48cf-a6b7-985ff6af492c
 */
#define BT_UUID_ARES_SRV_VAL                                                   \
    BT_UUID_128_ENCODE(0xf2765f1d, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

/**
 * @brief Ares bandwidth UUID.
 */
#define BT_UUID_ARES_SRV_BANDWIDTH_VAL                                         \
    BT_UUID_128_ENCODE(0xf2765f1e, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

/**
 * @brief Ares center frequency UUID.
 */
#define BT_UUID_ARES_SRV_CENTER_FREQ_VAL                                       \
    BT_UUID_128_ENCODE(0xf2765f1f, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

#define BT_UUID_ARES_SRV_REF_LEVEL_VAL                                         \
    BT_UUID_128_ENCODE(0xf2765f20, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

#define BT_UUID_ARES_SRV_DURATION_VAL                                          \
    BT_UUID_128_ENCODE(0xf2765f21, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

#define BT_UUID_ARES_SRV_DESCRIPTION_VAL                                       \
    BT_UUID_128_ENCODE(0xf2765f22, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

#define BT_UUID_ARES_SRV_CONFIG_READ_VAL                                       \
    BT_UUID_128_ENCODE(0xf2765f23, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

#define BT_UUID_ARES_SRV_CONFIG_RESP_VAL                                       \
    BT_UUID_128_ENCODE(0xf2765f24, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

#define BT_UUID_ARES_SRV_START_VAL                                             \
    BT_UUID_128_ENCODE(0xf2765f25, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

#define BT_UUID_ARES_SRV_NEIGHBOR_STATE_VAL                                    \
    BT_UUID_128_ENCODE(0xf2765f26, 0xd570, 0x48cf, 0xa6b7, 0x985ff6af492c)

#define BT_UUID_ARES_SRV BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_VAL)
#define BT_UUID_ARES_SRV_BANDWIDTH                                             \
    BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_BANDWIDTH_VAL)
#define BT_UUID_ARES_SRV_CENTER_FREQ                                           \
    BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_CENTER_FREQ_VAL)
#define BT_UUID_ARES_SRV_REF_LEVEL                                             \
    BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_REF_LEVEL_VAL)
#define BT_UUID_ARES_SRV_DURATION                                              \
    BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_DURATION_VAL)
#define BT_UUID_ARES_SRV_DESCRIPTION                                           \
    BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_DESCRIPTION_VAL)
#define BT_UUID_ARES_SRV_CONFIG_READ                                           \
    BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_CONFIG_READ_VAL)
#define BT_UUID_ARES_SRV_CONFIG_RESP                                           \
    BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_CONFIG_RESP_VAL)
#define BT_UUID_ARES_SRV_START BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_START_VAL)
#define BT_UUID_ARES_SRV_NEIGHBOR_STATE                                        \
    BT_UUID_DECLARE_128(BT_UUID_ARES_SRV_NEIGHBOR_STATE_VAL)

enum ares_srv_configs {
    ARES_CONFIG_BANDWIDTH,
    ARES_CONFIG_CENTER_FREQ,
    ARES_CONFIG_REF_LEVEL,
    ARES_CONFIG_DURATION,
    ARES_CONFIG_DESCRIPTION,

    ARES_CONFIG_INVALID,
};

/**
 * @struct ares_service_cb
 * @brief Service callback configurations.
 */
struct ares_service_cb {
    /**
     * @brief Callback for updating the bandwidth value.
     *
     * @param[in] conn Pointer to the bt_conn instance the write occurred on.
     * @param[in] bandwidth The new bandwidth.
     *
     * @note The bandwidth is supposed to be an 8-bit float.
     */
    void (*bandwidth_update)(struct bt_conn *conn, uint64_t bandwidth);

    /**
     * @brief Callback for updating the center frequency value.
     *
     * @param[in] conn Pointer to the bt_conn instance the write occurred on.
     * @param[in] center_freq The new center frequency.
     *
     * @note The center frequency is supposed to be an 8-bit float.
     */
    void (*center_frequency_update)(struct bt_conn *conn, uint64_t center_freq);

    /**
     * @brief Callback for updating the reference level value.
     *
     * @param[in] conn Pointer to the bt_conn instance the write occurred on.
     * @param[in] ref_level The new reference level.
     *
     * @note The reference level is supposed to be an 8-bit float.
     */
    void (*reference_level_update)(struct bt_conn *conn, uint64_t ref_level);

    /**
     * @brief Callback for updating the duration value.
     *
     * @param[in] conn Pointer to the bt_conn instance the write occurred on.
     * @param[in] duration The new duration.
     *
     * @note The duration is in seconds.
     */
    void (*duration_update)(struct bt_conn *conn, uint32_t duration);

    /**
     * @brief Callback for updating the test description.
     *
     * @param[in] conn Pointer to the bt_conn instance the write occurred on.
     * @param[in] buf Pointer to buffer that contains the description.
     * @param[in] len The length of the buffer.
     */
    void (*description_update)(struct bt_conn *conn, const void *buf,
                               uint16_t len);

    /**
     * @brief Callback for indicating a configuration needs to be read.
     *
     * @param[in] conn Pointer to the bt_conn instance the write occurred on.
     * @param[in] config The config to be read.
     *
     * @note The response should be carried out with the response indication.
     */
    void (*config_read)(struct bt_conn *conn, enum ares_srv_configs config);

    /**
     * @brief Callback for indicating that the config response has been
     * subscribed to.
     *
     * @param[in] enabled `true` if subscribed to to, `false` otherwise.
     */
    void (*config_response_ind_enabled)(bool enabled);

    /**
     * @brief Indication complete callback for config response characteristic.
     *
     * @param[in] conn Pointer to the bt_conn instance the indication was
     * carried out on.
     * @param[in] err The error code.
     */
    void (*config_response_ind_cb)(struct bt_conn *conn, uint8_t err);

    /**
     * @brief Callback for indication to start the data collection run.
     * @param[in] conn Pointer to the bt_conn instance the write occurred on.
     */
    void (*start)(struct bt_conn *conn);

    /**
     * @brief Callback for indicating that the neighbor state characteristic has
     * been subscribed to.
     *
     * @param[in] enabled `true` if subscribed to to, `false` otherwise.
     */
    void (*neighbor_state_enabled)(bool enabled);
};

/**
 * Initialize the ares service.
 *
 * @param[in] cb Pointer to the callback configuration.
 *
 * @return -EINVAL if cb is @p NULL.
 * @return 0 if no error.
 */
int bt_ares_srv_init(const struct ares_service_cb *cb);

/**
 *
 * @param conn Pointer to the bt_conn to send the indication on.
 * @param data The configuration data.
 * @param len Length of the data.
 *
 * @return @p -EACCESS if the indication has not been subscribed to.
 * @return @p 0 on success.
 * @return negative error code otherwise.
 */
int bt_ares_config_response(struct bt_conn *conn, const void *data, size_t len);

#endif // ARES_ARES_SERVICE_H
