/**
 * @file lora_backend.h
 *
 * @brief
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

extern const struct ares_lora_transport_api ares_lora_transport_api;

#define LORA_BACKEND_TX_RINGBUF_SIZE 256
#define LORA_BACKEND_RX_RINGBUF_SIZE 256

struct lora_common {
    const struct device *dev;
    lora_transport_handler_t handler;
    void *context;
    struct lora_modem_config config;
};

struct lora_async_driven {
    struct lora_common common;
    struct ring_buf tx_ringbuf;
    struct ring_buf rx_ringbuf;
    uint8_t tx_buf[LORA_BACKEND_TX_RINGBUF_SIZE];
    uint8_t rx_buf[LORA_BACKEND_RX_RINGBUF_SIZE];
    atomic_t tx_busy;
};

#define LORA_STRUCT struct lora_async_driven

#define LORA_DEFINE(_name)                                                     \
    static LORA_STRUCT UTIL_CAT(_name, _backend_lora);                         \
    static struct ares_lora_transport _name = {                                \
        .api = &ares_lora_transport_api,                                       \
        .ctx = &UTIL_CAT(_name, _backend_lora)}

const struct ares_lora *ares_lora_backend_lora_get_ptr(void);

#endif // ARES_LORA_BACKEND_H