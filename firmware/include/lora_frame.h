/**
 * @file lora_frame.h
 *
 * @brief
 *
 * @date 3/19/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */


#ifndef ARES_LORA_FRAME_H
#define ARES_LORA_FRAME_H

#include <zephyr/kernel.h>

struct lora_frame {
    uint8_t seq_cnt;
    uint64_t id;
    int64_t second;
    uint64_t nanosecond;
};

enum lora_frame_check_result {
    LORA_FRAME_CHECK_OK,
    LORA_FRAME_INVALID,
    LORA_FRAME_BAD_CRC,
};

int serialize_lora_frame(uint8_t *buf, size_t len, const struct lora_frame *frame);
// todo: check if this works locally
enum lora_frame_check_result check_lora_frame(const uint8_t *buf, size_t len);
int deserialize_lora_frame(const uint8_t *buf, size_t len, struct lora_frame *frame);

extern const size_t lora_frame_size;

#endif //ARES_LORA_FRAME_H