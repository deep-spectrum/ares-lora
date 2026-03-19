/**
 * @file lora.h
 *
 * @brief
 *
 * @date 3/19/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LORA_H
#define ARES_LORA_H

#include <zephyr/kernel.h>

enum lora_payload_type {
    START_TIME,
};

struct lora_payload {
    enum lora_payload_type type;
    union {
        struct {
            int64_t second;
            uint64_t nanosecond;
        };
    };
};

int ares_lora_send(const struct lora_payload *data, uint8_t repeat_count,
                   k_timeout_t interval);

extern struct k_msgq lora_rx_q;

#endif // ARES_LORA_H