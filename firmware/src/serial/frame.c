/**
 * @file frame.c
 *
 * @brief
 *
 * @date 3/20/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <serial/frame.h>
#include <zephyr/kernel.h>

#define ARES_FRAME_HEADER_OFFSET UINT32_C(0)
#define ARES_FRAME_LEN_OFFSET                                                  \
    (ARES_FRAME_HEADER_OFFSET + ARES_FRAME_HEADER_OVERHEAD)
#define ARES_FRAME_TYPE_OFFSET (ARES_FRAME_LEN_OFFSET + ARES_FRAME_LEN_OVERHEAD)
#define ARES_FRAME_PAYLOAD_OFFSET                                              \
    (ARES_FRAME_TYPE_OFFSET + ARES_FRAME_TYPE_OVERHEAD)
#define ARES_FRAME_FOOTER_OFFSET(payload_len)                                  \
    (ARES_FRAME_PAYLOAD_OFFSET + (uint64_t)(payload_len))

static size_t ares_strlen(const char *s, size_t max_cnt) {
    size_t len = 0;
    if (s == NULL) {
        return 0;
    }

    if (max_cnt == 0) {
        max_cnt = UINT16_MAX;
    }

    for (const char *buf = s; *buf != '\0' && len < max_cnt; buf++) {
        len++;
    }

    return len;
}

static size_t calculate_frame_length(const struct ares_frame *frame) {
    size_t payload_len = 0;

    __ASSERT_NO_MSG(frame != NULL);

    switch (frame->type) {
    case ARES_FRAME_WHOAMI: {
        payload_len = ares_strlen(frame->payload.id, 0);
        break;
    }
    case ARES_FRAME_START: {
        payload_len = sizeof(frame->payload.timespec);
        break;
    default:
        __ASSERT(false, "Invalid frame type received");
    }
    }

    return payload_len + ARES_FRAME_OVERHEAD;
}

static void serialize(uint8_t *buf, const struct ares_frame *frame,
                      size_t frame_len) {
    uint64_t payload_len = frame_len - ARES_FRAME_OVERHEAD;
    uint8_t *payload = &buf[ARES_FRAME_PAYLOAD_OFFSET];
    buf[ARES_FRAME_HEADER_OFFSET] = ARES_FRAME_HEADER;
    buf[ARES_FRAME_TYPE_OFFSET] = (uint8_t)frame->type;
    (void)memcpy(&buf[ARES_FRAME_LEN_OFFSET], &payload_len,
                 ARES_FRAME_LEN_OVERHEAD);

    switch (frame->type) {
    case ARES_FRAME_WHOAMI: {
        (void)memcpy(payload, frame->payload.id, payload_len);
        break;
    }
    case ARES_FRAME_START: {
        (void)memcpy(payload, &frame->payload.timespec, payload_len);
        break;
    }
    case ARES_FRAME_FRAMING_ERROR: {
        uint32_t error_code = frame->payload.frame_error;
        (void)memcpy(payload, &error_code, sizeof(error_code));
        break;
    }
    default:
        __ASSERT(false, "Invalid frame type received");
    }

    buf[ARES_FRAME_FOOTER_OFFSET(payload_len)] = ARES_FRAME_FOOTER;
}

int ares_serialize_frame(uint8_t *buf, size_t len,
                         const struct ares_frame *frame) {
    size_t frame_len;

    if (buf == NULL || len == 0 || frame == NULL) {
        return -EINVAL;
    }

    frame_len = calculate_frame_length(frame);
    if (len < frame_len) {
        return -ENOBUFS;
    }

    serialize(buf, frame, frame_len);

    return (int)frame_len;
}

static uint16_t retrieve_payload_length(const uint8_t *buf) {
    __ASSERT_NO_MSG(buf != NULL);

    uint64_t len;
    (void)memcpy(&len, &buf[ARES_FRAME_LEN_OFFSET], ARES_FRAME_LEN_OVERHEAD);
    return len;
}

static void deserialize(struct ares_frame *frame, const uint8_t *buf) {
    __ASSERT_NO_MSG(buf != NULL);
    uint64_t payload_len = retrieve_payload_length(buf);

    frame->type = (enum ares_frame_type)buf[ARES_FRAME_TYPE_OFFSET];

    switch (frame->type) {
    case ARES_FRAME_START: {
        (void)memcpy(&frame->payload.timespec, &buf[ARES_FRAME_PAYLOAD_OFFSET],
                     payload_len);
        break;
    }
    case ARES_FRAME_WHOAMI: {
        // nop
        break;
    }
    default: {
        __ASSERT(false, "Invalid frame type for deserialization.");
        break;
    }
    }
}

int ares_deserialize_frame(struct ares_frame *frame, const uint8_t *buf,
                           size_t len) {
    if (!ares_check_if_frame(buf, len)) {
        return -EBADMSG;
    }

    if (frame == NULL) {
        // buf and len already checked in above if statement
        return -EINVAL;
    }

    deserialize(frame, buf);

    return 0;
}

int ares_serial_frame_present(const uint8_t *buf, size_t len,
                              struct ares_frame_info *info) {
    int *start, *length, *remaining;
    size_t type_index, payload_length, footer_index;

    if (buf == NULL || info == NULL) {
        return -EINVAL;
    }

    // if -EINVAL, invalid parameters
    // if 0, no complete frame found
    // if 1, complete frame found

    start = &info->start_index;
    length = &info->frame_size;
    remaining = &info->bytes_left;

    for (size_t i = 0; i < len; i++) {

        if (buf[i] != ARES_FRAME_HEADER) {
            continue;
        }

        *start = (int)i;

        type_index = *start + ARES_FRAME_TYPE_OFFSET;
        if (type_index > len) {
            // cannot extract the length
            continue;
        }

        payload_length = retrieve_payload_length(&buf[*start]);

        footer_index = *start + ARES_FRAME_PAYLOAD_OFFSET + payload_length;
        if (footer_index >= len) {
            *remaining = (int)footer_index - ((int)len - *start) + 1;
            continue;
        }

        if (buf[footer_index] != ARES_FRAME_FOOTER) {
            // Not a frame
            continue;
        }

        *length = ARES_FRAME_OVERHEAD + payload_length;
        *remaining = 0;

        return 1;
    }

    return 0;
}

bool ares_check_if_frame(const uint8_t *buf, size_t len) {
    uint64_t payload_len;
    if (buf == NULL || len < ARES_FRAME_OVERHEAD) {
        return false;
    }

    if (buf[ARES_FRAME_HEADER_OFFSET] != ARES_FRAME_HEADER) {
        return false;
    }

    if (buf[ARES_FRAME_TYPE_OFFSET] >= ARES_FRAME_TYPE_INVALID) {
        return false;
    }

    payload_len = retrieve_payload_length(buf);

    if (len < (payload_len + ARES_FRAME_OVERHEAD)) {
        return false;
    }

    if (buf[ARES_FRAME_FOOTER_OFFSET(payload_len)] != ARES_FRAME_FOOTER) {
        return false;
    }

    return true;
}
