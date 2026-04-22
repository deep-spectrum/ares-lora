/**
 * @file serial.h
 *
 * @brief Serial API.
 *
 * @date 3/20/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_SERIAL_H
#define ARES_SERIAL_H

#include <serial/frame.h>
#include <serial/serial_common.h>
#include <stdbool.h>
#include <stddef.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

/**
 * @enum serial_transport_evt
 * @brief Serial transport events.
 */
enum serial_transport_evt {
    SERIAL_TRANSPORT_EVT_RX_RDY, ///< Rx ready.
    SERIAL_TRANSPORT_EVT_TX_RDY, ///< Tx ready.
};

/**
 * @brief Serial transport event handler prototype.
 *
 * @param[in] evt The event that occurred.
 * @param[in] ctx Pointer to the event context.
 */
typedef void (*serial_transport_handler_t)(enum serial_transport_evt evt,
                                           void *ctx);

#include <serial/serial_backend.h>

struct ares_serial;
struct ares_serial_transport;

/**
 * @struct ares_serial_transport_api
 * @brief Unified serial transport interface.
 */
struct ares_serial_transport_api {
    /**
     * @brief Function for initializing the serial transport interface.
     *
     * @param[in] transport Pointer to the transfer instance.
     * @param[in] config Pointer to the instance configuration.
     * @param[in] evt_handler  Event handler.
     * @param[in] context Pointer to the context passed to the event handler.
     *
     * @return 0 onsuccess.
     * @return negative error code otherwise.
     */
    int (*init)(const struct ares_serial_transport *transport,
                const void *config, serial_transport_handler_t evt_handler,
                void *context);

    /**
     * @brief Function for uninitializing the serial transport interface.
     *
     * @param[in] transport Pointer ot the transfer instance.
     *
     * @return 0 on success.
     * @return negative error code otherwise.
     */
    int (*uninit)(const struct ares_serial_transport *transport);

    /**
     * @brief Function for enabling transport in given TX mode.
     *
     * Function can be used to reconfigure TX to work in blocking mode.
     *
     * @param[in] transport Pointer ot the transfer instance.
     * @param[in] blocking_tx If `true`, the transport TX is enabled in blocking
     * mode.
     *
     * @return 0 on success.
     * @return negative error code otherwise.
     */
    int (*enable)(const struct ares_serial_transport *transport,
                  bool blocking_tx);

    /**
     * @brief Function for writing data to the transfer interface.
     *
     * @param[in] transport Pointer ot the transfer instance.
     * @param[in] data Pointer to the source buffer.
     * @param[in] length Source buffer length.
     * @param[out] cnt Pointer to the sent bytes counter.
     *
     * @return 0 on success.
     * @return negative error code otherwise.
     */
    int (*write)(const struct ares_serial_transport *transport,
                 const void *data, size_t length, size_t *cnt);

    /**
     * @brief Function for reading data from the transfer interface.
     *
     * @param[in] transport Pointer ot the transfer instance.
     * @param[in] data Pointer to the destination buffer.
     * @param[in] length Destination buffer length.
     * @param[out] cnt Pointer to the received bytes counter.
     *
     * @return 0 on success.
     * @return negative error code otherwise.
     */
    int (*read)(const struct ares_serial_transport *transport, void *data,
                size_t length, size_t *cnt);

    /**
     * @brief Function that blocks execution of the thread until the DTR control
     * line is asserted.
     *
     * @param[in] transport Pointer ot the transfer instance.
     */
    void (*wait_dtr)(const struct ares_serial_transport *transport);

    /**
     * @brief Function that checks if there is a reception error.
     *
     * @param[in] transport Pointer ot the transfer instance.
     *
     * @return `true` if there was a reception error.
     * @return `false` if there was no reception error.
     */
    bool (*rx_error)(const struct ares_serial_transport *transport);

    /**
     * @brief Function that configures the transport to operate in
     * blocking/non-blocking mode while waiting for a host connection.
     *
     * @param[in] transport Pointer ot the transfer instance.
     * @param[in] block If `true`, transport will block until a host connection
     * is established.
     */
    void (*block_no_usb_host)(const struct ares_serial_transport *transport,
                              bool block);
};

/**
 * @struct ares_serial_transport
 * @brief Transfer interface.
 */
struct ares_serial_transport {
    /**
     * Transfer interface API.
     */
    const struct ares_serial_transport_api *api;

    /**
     * Transfer interface context.
     */
    void *ctx;
};

/**
 * @enum ares_serial_signal
 * @brief Signals for the serial interface.
 */
enum ares_serial_signal {
    ARES_SIGNAL_RXRDY,  ///< Rx data ready.
    ARES_SIGNAL_TXDONE, ///< Done transmitting. Must be last one before
                        ///< ARES_SIGNALS.

    ARES_SIGNALS, ///< Last enumerator.
};

/**
 * Unified serial buffer size for transmitting and receiving.
 */
#define ARES_SERIAL_TRX_BUF_SIZE                                               \
    MAX(SERIAL_BACKEND_TX_RINGBUF_SIZE, SERIAL_BACKEND_RX_RINGBUF_SIZE)

/**
 * @struct ares_buf
 * @brief Unified buffer structure for serial.
 */
struct ares_buf {
    /**
     * Buffer for storing bytes.
     */
    uint8_t buf[ARES_SERIAL_TRX_BUF_SIZE];

    /**
     * Number of valid bytes starting from 0.
     */
    size_t len;
};

/**
 * @struct ares_serial_command
 * @brief Ares frame handler descriptor.
 */
struct ares_serial_command {
    /**
     * The frame type to handle.
     */
    enum ares_frame_type command;

    /**
     * The frame handler.
     *
     * @param[in] serial Pointer to serial instance.
     * @param[in] frame Pointer to frame received.
     */
    void (*callback)(const struct ares_serial *serial,
                     struct ares_frame *frame);
};

/**
 * @struct ares_serial_ctx
 * @brief Ares serial instance context.
 */
struct ares_serial_ctx {
    /**
     * Serial receive buffer.
     */
    struct ares_buf rx_buf;

    /**
     * Serial transmit buffer.
     */
    struct ares_buf tx_buf;

    /**
     * Serial event signals.
     */
    struct k_poll_signal signals[ARES_SIGNALS];

    /**
     * Serial poll events.
     */
    struct k_poll_event events[ARES_SIGNALS];

    /**
     * Array of serial handlers for received frames.
     */
    const struct ares_serial_command *commands;

    /**
     * Number of serial handlers.
     */
    size_t num_commands;

    /**
     * Write mutex.
     */
    struct k_mutex wr_mtx;

    /**
     * Thread ID for serial.
     */
    k_tid_t tid;
};

/**
 * @struct ares_serial
 * @brief Ares serial internals.
 */
struct ares_serial {
    /**
     * Transport instance.
     */
    const struct ares_serial_transport *iface;

    /**
     * Internal context.
     */
    struct ares_serial_ctx *ctx;

    /**
     * Instance name.
     */
    const char *name;

    /**
     * Instance thread.
     */
    struct k_thread *thread;

    /**
     * Thread stack.
     */
    k_thread_stack_t *stack;
};

/**
 * @brief Macro for defining an ares_serial instance.
 *
 * @param _name Instance name.
 * @param _transport Pointer to the transport interface.
 */
#define ARES_SERIAL_DEFINE(_name, _transport)                                  \
    static struct ares_serial_ctx UTIL_CAT(_name, _ctx);                       \
    static K_KERNEL_STACK_DEFINE(UTIL_CAT(_name, _stack),                      \
                                 CONFIG_ARES_SERIAL_STACK_SIZE);               \
    static struct k_thread UTIL_CAT(_name, _thread);                           \
    static const struct ares_serial _name = {.iface = (_transport),            \
                                             .ctx = &UTIL_CAT(_name, _ctx),    \
                                             .name = STRINGIFY(_name),         \
                                             .thread =                         \
                                                 &UTIL_CAT(_name, _thread),    \
                                             .stack = UTIL_CAT(_name, _stack)}

/**
 * @brief Function for initiating a transport layer and internal serial state.
 *
 * @param[in] serial Pointer to serial instance.
 * @param[in] transport_config Transport configuration during initialization.
 *
 * @return 0 on success.
 * @return negative error code otherwise.
 */
int ares_serial_init(const struct ares_serial *serial,
                     const void *transport_config);

/**
 * @brief Function for registering frame handlers.
 *
 * @param[in] serial Pointer to serial instance.
 * @param[in] commands The array of frame handlers.
 * @param[in] num_commands The number of frame handlers in the array.
 *
 * @return 0 on success.
 * @return -EINVAL if parameters are invalid.
 */
int ares_serial_register_command_callbacks(
    const struct ares_serial *serial,
    const struct ares_serial_command *commands, size_t num_commands);

/**
 * @brief Function to write a frame to the serial transfer layer.
 *
 * @param[in] serial Pointer to serial instance.
 * @param[in] frame The frame to write to the serial transport.
 *
 * @return 0 on success.
 * @return -EINVAL if parameters are invalid.
 * @return negative error code otherwise.
 */
int ares_serial_write_frame(const struct ares_serial *serial,
                            const struct ares_frame *frame);

/**
 * @brief Function to flush the serial transfer layer transmit buffer.
 *
 * @param[in] serial Pointer to serial instance.
 * @param[in] timeout The maximum amount of time to wait for the flush to occur.
 * Use K_FOREVER to wait indefinitely.
 */
void ares_serial_flush_out(const struct ares_serial *serial,
                           k_timeout_t timeout);

/**
 * @brief Block current thread execution until a connection is established with
 * a host.
 *
 * @param[in] serial Pointer to serial instance.
 *
 * @return 0 on success.
 * @return -EINVAL on invalid parameters.
 * @return -ENOTSUP if API is not supported.
 */
int wait_serial_ready(const struct ares_serial *serial);

/**
 * @brief Function to configure the transport layer to block transmissions if no
 * host is connected.
 *
 * @param[in] serial Pointer to serial instance.
 * @param[in] block Tell transfer layer to block transmissions until host is
 * connected.
 *
 * @return 0 on success.
 * @return -EINVAL if parameters are invalid.
 */
int set_wait_usb_host(const struct ares_serial *serial, bool block);

/**
 * @brief Function to check if there was a reception error in the transfer
 * layer.
 *
 * @param[in] serial Pointer to serial instance.
 * @return `true` if invalid parameters passed in or if there was a reception
 * error.
 * @return `false` otherwise.
 */
bool ares_serial_check_rx_error(const struct ares_serial *serial);

#endif // ARES_SERIAL_H