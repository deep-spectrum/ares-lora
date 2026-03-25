/**
 * @file lora.h
 *
 * @brief
 *
 * @date 3/24/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LORA_H
#define ARES_LORA_H

#include <zephyr/kernel.h>

enum lora_transport_evt {
    LORA_TRANSPORT_EVT_RX_RDY,
    LORA_TRANSPORT_EVT_TX_RDY,
};

typedef void (*lora_transport_handler_t)(enum lora_transport_evt evt,
                                         void *ctx);

#include <lora/lora_backend.h>

struct ares_lora;
struct ares_lora_transport;

struct ares_lora_transport_api {
    int (*init)(const struct ares_lora_transport *transport, const void *config,
                lora_transport_handler_t evt_handler, void *context);
    int (*uninit)(const struct ares_lora_transport *transport);
    int (*enable)(const struct ares_lora_transport *transport, bool block_tx);
    int (*write)(const struct ares_lora_transport *transport, const void *data,
                 size_t length, size_t *cnt);
    int (*read)(const struct ares_lora_transport *transport, void *data,
                size_t length, size_t *cnt);
};

struct ares_lora_transport {
    const struct ares_lora_transport_api *api;
    void *ctx;
};

enum ares_lora_signal {
    ARES_LORA_SIGNAL_RXRDY,
    ARES_LORA_SIGNAL_TXDONE,

    ARES_LORA_SIGNALS,
};

#define ARES_LORA_TRX_BUF_SIZE 256
struct ares_lora_buf {
    uint8_t buf[ARES_LORA_TRX_BUF_SIZE + 1];
    size_t len;
};

struct ares_lora_command {
    // todo: packet type
    // todo: handler
};

struct ares_lora_ctx {
    struct ares_lora_buf rx_buf;
    struct ares_lora_buf tx_buf;

    struct k_poll_signal signals[ARES_LORA_SIGNALS];
    struct k_poll_event events[ARES_LORA_SIGNALS];

    const struct ares_lora_command *commands;
    size_t num_commands;

    struct k_mutex wr_mtx;
    k_tid_t tid;
};

struct ares_lora {
    const struct ares_lora_transport *iface;
    struct ares_lora_ctx *ctx;
    const char *name;
    struct k_thread *thread;
    k_thread_stack_t *stack;
};

#define ARES_LORA_DEFINE(_name, _transport)                                    \
    static struct ares_lora_ctx UTIL_CAT(_name, _ctx);                         \
    static K_KERNEL_STACK_DEFINE(UTIL_CAT(_name, _stack),                      \
                                 CONFIG_ARES_LORA_STACK_SIZE);                 \
    static struct k_thread UTIL_CAT(_name, _thread);                           \
    static const struct ares_lora _name = {.iface = (_transport),              \
                                           .ctx = &UTIL_CAT(_name, _ctx),      \
                                           .name = STRINGIFY(_name),           \
                                           .thread =                           \
                                               &UTIL_CAT(_name, _thread),      \
                                           .stack = UTIL_CAT(_name, _stack)}

int ares_lora_init(const struct ares_lora *lora, const void *transport_config);

#endif // ARES_LORA_H