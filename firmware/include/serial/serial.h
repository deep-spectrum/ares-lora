/**
 * @file serial.h
 *
 * @brief
 *
 * @date 3/20/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_SERIAL_H
#define ARES_SERIAL_H

#include <serial/frame.h>
#include <stdbool.h>
#include <stddef.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

enum serial_transport_evt {
    SERIAL_TRANSPORT_EVT_RX_RDY,
    SERIAL_TRANSPORT_EVT_TX_RDY,
};

typedef void (*serial_transport_handler_t)(enum serial_transport_evt evt,
                                           void *ctx);

#include <serial/serial_backend.h>

struct ares_serial;
struct ares_serial_transport;

struct ares_serial_transport_api {
    int (*init)(const struct ares_serial_transport *transport,
                const void *config, serial_transport_handler_t evt_handler,
                void *context);
    int (*uninit)(const struct ares_serial_transport *transport);
    int (*enable)(const struct ares_serial_transport *transport,
                  bool blocking_tx);
    int (*write)(const struct ares_serial_transport *transport,
                 const void *data, size_t length, size_t *cnt);
    int (*read)(const struct ares_serial_transport *transport, void *data,
                size_t length, size_t *cnt);
    void (*wait_dtr)(const struct ares_serial_transport *transport);
    bool (*rx_error)(const struct ares_serial_transport *transport);
    void (*block_no_usb_host)(const struct ares_serial_transport *transport,
                              bool block);
};

struct ares_serial_transport {
    const struct ares_serial_transport_api *api;
    void *ctx;
};

enum ares_serial_signal {
    ARES_SIGNAL_RXRDY,
    ARES_SIGNAL_TXDONE,

    ARES_SIGNALS,
};

#define ARES_SERIAL_TRX_BUF_SIZE 256
struct ares_buf {
    uint8_t buf[ARES_SERIAL_TRX_BUF_SIZE + 1];
    size_t len;
};

struct ares_serial_command {
    enum ares_frame_type command;
    void (*callback)(struct ares_serial *serial, struct ares_frame *frame);
};

struct ares_serial_ctx {
    struct ares_buf rx_buf;
    struct ares_buf tx_buf;

    struct k_poll_signal signals[ARES_SIGNALS];
    struct k_poll_event events[ARES_SIGNALS];

    struct ares_serial_command *commands;
    size_t num_commands;

    struct k_mutex wr_mtx;
    k_tid_t tid;
};

struct ares_serial {
    const struct ares_serial_transport *iface;
    struct ares_serial_ctx *ctx;
    const char *name;
    struct k_thread *thread;
    k_thread_stack_t *stack;
};

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

int ares_serial_init(const struct ares_serial *serial,
                     const void *transport_config);

int ares_serial_register_command_callbacks(const struct ares_serial *serial, const struct ares_serial_command *commands, size_t num_commands);
int ares_serial_write_frame(const struct ares_serial *serial, const struct ares_frame *frame);
void ares_serial_flush_out(const struct ares_serial *serial);
int wait_serial_ready(const struct ares_serial *serial);
int set_wait_usb_host(const struct ares_serial *serial, bool block);
bool ares_serial_check_rx_error(const struct ares_serial *serial);

#endif // ARES_SERIAL_H