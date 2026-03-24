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

#endif // ARES_LORA_H