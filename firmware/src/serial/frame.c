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
    case ARES_FRAME_SETTING: {
        payload_len = SIZEOF_FIELD(struct ares_frame, payload.SETTING.setting) +
                      SIZEOF_FIELD(struct ares_frame, payload.SETTING.value);
        break;
    }
    case ARES_FRAME_START: {
        payload_len = SIZEOF_FIELD(struct ares_frame, payload.START.sec) +
                      SIZEOF_FIELD(struct ares_frame, payload.START.ns) +
                      SIZEOF_FIELD(struct ares_frame, payload.START.id) +
                      SIZEOF_FIELD(struct ares_frame, payload.START.broadcast) +
                      SIZEOF_FIELD(struct ares_frame, payload.START.seq_cnt) +
                      SIZEOF_FIELD(struct ares_frame, payload.START.packet_id);
        break;
    }
    case ARES_FRAME_ACK: {
        payload_len = SIZEOF_FIELD(struct ares_frame, payload.ACK);
        break;
    }
    case ARES_FRAME_LED: {
        payload_len = SIZEOF_FIELD(struct ares_frame, payload.LED);
        break;
    }
    case ARES_FRAME_FRAMING_ERROR: {
        payload_len = SIZEOF_FIELD(struct ares_frame, payload.FRAMING_ERROR);
        break;
    }
    default: {
        __ASSERT(false, "Invalid frame type received");
        break;
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
    case ARES_FRAME_SETTING: {
        (void)memcpy(payload, &frame->payload.SETTING.setting,
                     SIZEOF_FIELD(struct ares_frame, payload.SETTING.setting));
        (void)memcpy(
            payload + SIZEOF_FIELD(struct ares_frame, payload.SETTING.setting),
            &frame->payload.SETTING.value,
            SIZEOF_FIELD(struct ares_frame, payload.SETTING.value));
        break;
    }
    case ARES_FRAME_START: {
        (void)memcpy(payload, &frame->payload.START.sec,
                     SIZEOF_FIELD(struct ares_frame, payload.START.sec));
        (void)memcpy(payload +
                         SIZEOF_FIELD(struct ares_frame, payload.START.sec),
                     &frame->payload.START.ns,
                     SIZEOF_FIELD(struct ares_frame, payload.START.ns));
        (void)memcpy(payload +
                         SIZEOF_FIELD(struct ares_frame, payload.START.sec) +
                         SIZEOF_FIELD(struct ares_frame, payload.START.ns),
                     &frame->payload.START.id,
                     SIZEOF_FIELD(struct ares_frame, payload.START.id));
        (void)memcpy(payload +
                         SIZEOF_FIELD(struct ares_frame, payload.START.sec) +
                         SIZEOF_FIELD(struct ares_frame, payload.START.ns) +
                         SIZEOF_FIELD(struct ares_frame, payload.START.id),
                     &frame->payload.START.broadcast,
                     SIZEOF_FIELD(struct ares_frame, payload.START.broadcast));
        (void)memcpy(
            payload + SIZEOF_FIELD(struct ares_frame, payload.START.sec) +
                SIZEOF_FIELD(struct ares_frame, payload.START.ns) +
                SIZEOF_FIELD(struct ares_frame, payload.START.id) +
                SIZEOF_FIELD(struct ares_frame, payload.START.broadcast),
            &frame->payload.START.seq_cnt,
            SIZEOF_FIELD(struct ares_frame, payload.START.seq_cnt));
        (void)memcpy(
            payload + SIZEOF_FIELD(struct ares_frame, payload.START.sec) +
                SIZEOF_FIELD(struct ares_frame, payload.START.ns) +
                SIZEOF_FIELD(struct ares_frame, payload.START.id) +
                SIZEOF_FIELD(struct ares_frame, payload.START.broadcast) +
                SIZEOF_FIELD(struct ares_frame, payload.START.seq_cnt),
            &frame->payload.START.packet_id,
            SIZEOF_FIELD(struct ares_frame, payload.START.packet_id));
        break;
    }
    case ARES_FRAME_ACK: {
        (void)memcpy(payload, &frame->payload.ACK,
                     SIZEOF_FIELD(struct ares_frame, payload.ACK));
        break;
    }
    case ARES_FRAME_LED: {
        (void)memcpy(payload, &frame->payload.LED,
                     SIZEOF_FIELD(struct ares_frame, payload.LED));
        break;
    }
    case ARES_FRAME_FRAMING_ERROR: {
        uint32_t error_code = frame->payload.FRAMING_ERROR;
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
    const uint8_t *payload = &buf[ARES_FRAME_PAYLOAD_OFFSET];
    uint64_t payload_len = retrieve_payload_length(buf);

    frame->type = (enum ares_frame_type)buf[ARES_FRAME_TYPE_OFFSET];

    switch (frame->type) {
    case ARES_FRAME_SETTING: {
        (void)memcpy(&frame->payload.SETTING.setting, payload,
                     SIZEOF_FIELD(struct ares_frame, payload.SETTING.setting));
        if (payload_len ==
            SIZEOF_FIELD(struct ares_frame, payload.SETTING.setting)) {
            frame->payload.SETTING.set = false;
        } else {
            frame->payload.SETTING.set = true;
            (void)memcpy(
                &frame->payload.SETTING.value,
                payload +
                    SIZEOF_FIELD(struct ares_frame, payload.SETTING.setting),
                SIZEOF_FIELD(struct ares_frame, payload.SETTING.value));
        }
        break;
    }
    case ARES_FRAME_START: {
        (void)memcpy(&frame->payload.START.sec, payload,
                     SIZEOF_FIELD(struct ares_frame, payload.START.sec));
        (void)memcpy(&frame->payload.START.ns,
                     payload +
                         SIZEOF_FIELD(struct ares_frame, payload.START.sec),
                     SIZEOF_FIELD(struct ares_frame, payload.START.ns));
        (void)memcpy(&frame->payload.START.id,
                     payload +
                         SIZEOF_FIELD(struct ares_frame, payload.START.sec) +
                         SIZEOF_FIELD(struct ares_frame, payload.START.ns),
                     SIZEOF_FIELD(struct ares_frame, payload.START.id));
        (void)memcpy(&frame->payload.START.broadcast,
                     payload +
                         SIZEOF_FIELD(struct ares_frame, payload.START.sec) +
                         SIZEOF_FIELD(struct ares_frame, payload.START.ns) +
                         SIZEOF_FIELD(struct ares_frame, payload.START.id),
                     SIZEOF_FIELD(struct ares_frame, payload.START.broadcast));
        // receive side: we don't care about the seq_cnt or packet_id...
        break;
    }
    case ARES_FRAME_LORA_CONFIG: {
        (void)memcpy(&frame->payload.LORA_CONFIG, payload, payload_len);
        break;
    }
    case ARES_FRAME_LED: {
        (void)memcpy(&frame->payload.LED, payload, payload_len);
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
