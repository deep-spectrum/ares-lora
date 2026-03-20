/**
 * @file serial_backend.h
 *
 * @brief
 *
 * @date 3/20/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_SERIAL_BACKEND_H
#define ARES_SERIAL_BACKEND_H

#include <serial/serial.h>
#include <zephyr/sys/ring_buffer.h>

extern const struct ares_serial_transport_api serial_uart_transport_api;

#define SERIAL_BACKEND_TX_RINGBUF_SIZE 256
#define SERIAL_BACKEND_RX_RINGBUF_SIZE 256

struct serial_uart_common {
    const struct device *dev;
    serial_transport_handler_t handler;
    void *context;
    bool block_tx;
    atomic_t block_no_usb;
};

struct serial_uart_int_driven {
    struct serial_uart_common common;
    struct ring_buf tx_ringbuf;
    struct ring_buf rx_ringbuf;
    uint8_t tx_buf[SERIAL_BACKEND_TX_RINGBUF_SIZE];
    uint8_t rx_buf[SERIAL_BACKEND_RX_RINGBUF_SIZE];
    struct k_timer dtr_timer;
    atomic_t tx_busy;
};

#define SERIAL_UART_STRUCT struct serial_uart_int_driven

#define SERIAL_UART_DEFINE(_name)                                              \
    static SERIAL_UART_STRUCT _name##_backend_uart;                            \
    static struct ares_serial_transport _name = {                              \
        .api = &serial_uart_transport_api, .ctx = &_name##_backend_uart}

const struct ares_serial *ares_serial_backend_uart_get_ptr(void);

#endif // ARES_SERIAL_BACKEND_H