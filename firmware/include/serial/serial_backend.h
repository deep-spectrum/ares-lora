/**
 * @file serial_backend.h
 *
 * @brief Serial transport layer header.
 *
 * @date 3/20/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_SERIAL_BACKEND_H
#define ARES_SERIAL_BACKEND_H

#include <serial/serial.h>
#include <serial/serial_common.h>
#include <zephyr/sys/ring_buffer.h>

/**
 * Serial transport API.
 */
extern const struct ares_serial_transport_api ares_serial_uart_transport_api;

/**
 * @struct serial_uart_common
 * @brief Serial transfer layer common internals.
 */
struct serial_uart_common {
    /**
     * Pointer to device instance.
     */
    const struct device *dev;

    /**
     * Transport layer event handler.
     */
    serial_transport_handler_t handler;

    /**
     * Transport event context.
     */
    void *context;

    /**
     * Flag indicating if TX should be blocking.
     */
    bool block_tx;

    /**
     * Flag indicating if Tx should block if there is no host.
     */
    atomic_t block_no_usb;
};

/**
 * @struct serial_uart_int_driven
 * @brief Serial interrupt driven transport layer internals.
 */
struct serial_uart_int_driven {
    /**
     * Common internals.
     */
    struct serial_uart_common common;

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
    uint8_t tx_buf[SERIAL_BACKEND_TX_RINGBUF_SIZE];

    /**
     * Rx buffer.
     */
    uint8_t rx_buf[SERIAL_BACKEND_RX_RINGBUF_SIZE];

    /**
     * Timer for checking if the DTR line is asserted.
     */
    struct k_timer dtr_timer;

    /**
     * Flag indicating that the transmitter is busy.
     */
    atomic_t tx_busy;
};

/**
 * Serial transport implementation struct.
 */
#define SERIAL_UART_STRUCT struct serial_uart_int_driven

/**
 * @brief Macro for creating a serial transport instance.
 * @param _name The instance name.
 */
#define SERIAL_UART_DEFINE(_name)                                              \
    static SERIAL_UART_STRUCT _name##_backend_uart;                            \
    static struct ares_serial_transport _name = {                              \
        .api = &ares_serial_uart_transport_api, .ctx = &_name##_backend_uart}

/**
 * @brief Provides pointer to the ares_serial backend instance.
 * @return Pointer to the backend serial instance.
 */
const struct ares_serial *ares_serial_backend_uart_get_ptr(void);

#endif // ARES_SERIAL_BACKEND_H