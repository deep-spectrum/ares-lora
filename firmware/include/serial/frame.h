/**
 * @file frame.h
 *
 * @brief
 *
 * @date 3/19/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */


#ifndef ARES_FRAME_H
#define ARES_FRAME_H

#include <zephyr/kernel.h>

enum ares_frame_type {
    ARES_FRAME_WHOAMI,
    ARES_FRAME_START,

    ARES_FRAME_TYPE_INVALID,
};

struct ares_frame {
    enum ares_frame_type type;
    union {
        const char *id; ///< ARES_FRAME_WHOAMI (TX only), NULL terminated string.
        struct {
            int64_t sec;
            uint64_t ns;
        } timespec; /// < ARES_FRAME_START (RX/TX)
    } payload;
};

int ares_serialize_frame(uint8_t *buf, size_t len, const struct ares_frame *frame);
int ares_deserialize_frame(struct ares_frame *frame, const uint8_t *buf, size_t len);
bool ares_check_if_frame(const uint8_t *buf, size_t len);

#endif //ARES_FRAME_H