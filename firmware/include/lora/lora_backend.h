/**
 * @file lora_backend.h
 *
 * @brief LoRa transport layer header.
 *
 * @date 3/24/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LORA_BACKEND_H
#define ARES_LORA_BACKEND_H

#include <lora/lora.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>

/**
 * LoRa transport API.
 */
extern const struct ares_lora_transport_api ares_lora_transport_api;

#if defined(CONFIG_ARES_LORA_TX_RINGBUF_SIZE)
/**
 * Buffer size for the Tx ring buffer.
 */
#define LORA_BACKEND_TX_RINGBUF_SIZE CONFIG_ARES_LORA_TX_RINGBUF_SIZE
#else
/**
 * Buffer size for the Tx ring buffer.
 */
#define LORA_BACKEND_TX_RINGBUF_SIZE 512
#endif

#if defined(CONFIG_ARES_LORA_RX_RINGBUF_SIZE)
/**
 * Buffer size for the Rx ring buffer.
 */
#define LORA_BACKEND_RX_RINGBUF_SIZE CONFIG_ARES_LORA_RX_RINGBUF_SIZE
#else
/**
 * Buffer size for the Tx ring buffer.
 */
#define LORA_BACKEND_RX_RINGBUF_SIZE 512
#endif

/**
 * @struct lora_common
 * @brief LoRa transfer layer common internals.
 */
struct lora_common {
    /**
     * Pointer to device instance.
     */
    const struct device *dev;

    /**
     * Transport layer event handler.
     */
    lora_transport_handler_t handler;

    /**
     * Transport event context.
     */
    void *context;

    /**
     * Current LoRa configurations.
     */
    struct lora_modem_config config;
};

/**
 * @struct lora_async_driven
 * @brief LoRa async driven transport layer internals.
 */
struct lora_async_driven {
    /**
     * Common internals.
     */
    struct lora_common common;

    /**
     * Ring buffer for transmitting.
     */
    struct ring_buf tx_ringbuf;

    /**
     * Ring buffer for receiving.
     */
    struct ring_buf rx_ringbuf;

    /**
     * Tx buffer.
     */
    uint8_t tx_buf[LORA_BACKEND_TX_RINGBUF_SIZE];

    /**
     * Rx buffer.
     */
    uint8_t rx_buf[LORA_BACKEND_RX_RINGBUF_SIZE];

    /**
     * Flag indicating Tx is busy.
     */
    atomic_t tx_busy;
};

/**
 * LoRa transport implementation struct.
 */
#define LORA_STRUCT struct lora_async_driven

/**
 * @brief Macro for creating a lora transport instance.
 * @param[in] _name The instance name.
 */
#define LORA_DEFINE(_name)                                                     \
    static LORA_STRUCT UTIL_CAT(_name, _backend_lora);                         \
    static struct ares_lora_transport _name = {                                \
        .api = &ares_lora_transport_api,                                       \
        .ctx = &UTIL_CAT(_name, _backend_lora)}

/**
 * @brief Provides pointer to the ares_lora backend instance.
 * @return Pointer to the lora instance.
 */
const struct ares_lora *ares_lora_backend_lora_get_ptr(void);

#endif // ARES_LORA_BACKEND_H