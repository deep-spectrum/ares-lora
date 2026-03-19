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

struct lora_send_data {
    int64_t second;
    uint64_t nanosecond;
};

int ares_lora_send(const struct lora_send_data *data, uint8_t repeat_count,
                   k_timeout_t interval);

#endif // ARES_LORA_H