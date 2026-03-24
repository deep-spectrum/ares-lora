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
};

struct ares_lora_transport {
    const struct ares_lora_transport_api api;
    void *ctx;
};

#endif // ARES_LORA_H