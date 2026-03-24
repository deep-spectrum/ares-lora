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

#define ARES_FRAME_HEADER          '^'
#define ARES_FRAME_FOOTER          '@'

#define ARES_FRAME_HEADER_OVERHEAD UINT32_C(1)
#define ARES_FRAME_TYPE_OVERHEAD   UINT32_C(1)
#define ARES_FRAME_LEN_OVERHEAD    UINT32_C(2)
#define ARES_FRAME_FOOTER_OVERHEAD UINT32_C(1)
#define ARES_FRAME_OVERHEAD                                                    \
    (uint64_t)(ARES_FRAME_HEADER_OVERHEAD + ARES_FRAME_TYPE_OVERHEAD +         \
               ARES_FRAME_LEN_OVERHEAD + ARES_FRAME_FOOTER_OVERHEAD)

enum ares_frame_error {
    ARES_FRAME_ERROR_BAD_FRAME = 0,
    ARES_FRAME_ERROR_BAD_TYPE = 1,
    ARES_FRAME_ERROR_NOT_IMPLEMENTED = 2,
};

enum ares_frame_type {
    ARES_FRAME_WHOAMI,        ///< Who am I frame. No receive payload.
    ARES_FRAME_START,         ///< Start time frame.
    ARES_FRAME_FRAMING_ERROR, ///< Framing error frame. TX only.

    ARES_FRAME_TYPE_INVALID,
};

struct ares_frame {
    enum ares_frame_type type;
    union {
        const char
            *id; ///< ARES_FRAME_WHOAMI (TX only), NULL terminated string.
        struct {
            int64_t sec;
            uint64_t ns;
        } timespec; /// < ARES_FRAME_START (RX/TX)
        enum ares_frame_error
            frame_error; ///< ARES_FRAME_FRAMING_ERROR (TX only)
    } payload;
};

struct ares_frame_info {
    int start_index;
    int frame_size;
    int bytes_left;
};

int ares_serialize_frame(uint8_t *buf, size_t len,
                         const struct ares_frame *frame);
int ares_deserialize_frame(struct ares_frame *frame, const uint8_t *buf,
                           size_t len);
int ares_serial_frame_present(const uint8_t *buf, size_t len,
                              struct ares_frame_info *info);
bool ares_check_if_frame(const uint8_t *buf, size_t len);

#endif // ARES_FRAME_H