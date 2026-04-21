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
#include <serial/serial_common.h>
#include <zephyr/kernel.h>

#define ARES_FRAME_HEADER_OFFSET UINT32_C(0)
#define ARES_FRAME_LEN_OFFSET                                                  \
    (ARES_FRAME_HEADER_OFFSET + ARES_FRAME_HEADER_OVERHEAD)
#define ARES_FRAME_TYPE_OFFSET (ARES_FRAME_LEN_OFFSET + ARES_FRAME_LEN_OVERHEAD)
#define ARES_FRAME_PAYLOAD_OFFSET                                              \
    (ARES_FRAME_TYPE_OFFSET + ARES_FRAME_TYPE_OVERHEAD)
#define ARES_FRAME_FOOTER_OFFSET(payload_len)                                  \
    (ARES_FRAME_PAYLOAD_OFFSET + (uint64_t)(payload_len))

#define ARES_FRAME_MAX_SIZE                                                    \
    MIN(SERIAL_BACKEND_RX_RINGBUF_SIZE, SERIAL_BACKEND_TX_RINGBUF_SIZE)
#define ARES_FRAME_MAX_PAYLOAD_SIZE (ARES_FRAME_MAX_SIZE - ARES_FRAME_OVERHEAD)

#define FSIZEOF_FIELD(field)        SIZEOF_FIELD(struct ares_frame, payload.field)

static size_t calculate_frame_length(const struct ares_frame *frame) {
    size_t payload_len = 0;

    __ASSERT_NO_MSG(frame != NULL);

    switch (frame->type) {
    case ARES_FRAME_SETTING: {
        payload_len =
            FSIZEOF_FIELD(SETTING.setting) + FSIZEOF_FIELD(SETTING.value);
        break;
    }
    case ARES_FRAME_START: {
        payload_len = FSIZEOF_FIELD(START.sec) + FSIZEOF_FIELD(START.ns) +
                      FSIZEOF_FIELD(START.id) + FSIZEOF_FIELD(START.broadcast) +
                      FSIZEOF_FIELD(START.seq_cnt) +
                      FSIZEOF_FIELD(START.packet_id);
        break;
    }
    case ARES_FRAME_ACK: {
        payload_len = FSIZEOF_FIELD(ACK);
        break;
    }
    case ARES_FRAME_LED: {
        payload_len = FSIZEOF_FIELD(LED);
        break;
    }
    case ARES_FRAME_FRAMING_ERROR: {
        payload_len = FSIZEOF_FIELD(FRAMING_ERROR);
        break;
    }
    case ARES_FRAME_HEARTBEAT: {
        payload_len = FSIZEOF_FIELD(HEARTBEAT.flags) +
                      FSIZEOF_FIELD(HEARTBEAT.tx_count) +
                      FSIZEOF_FIELD(HEARTBEAT.id);
        break;
    }
    case ARES_FRAME_CLAIM: {
        payload_len = FSIZEOF_FIELD(CLAIM);
        break;
    }
    case ARES_FRAME_LOG: {
        payload_len = FSIZEOF_FIELD(LOG.broadcast) + FSIZEOF_FIELD(LOG.id) +
                      FSIZEOF_FIELD(LOG.tx_cnt) + FSIZEOF_FIELD(LOG.part) +
                      FSIZEOF_FIELD(LOG.num_parts) + FSIZEOF_FIELD(LOG.log_id);
        payload_len += frame->payload.LOG.msg_len;
        break;
    }
    case ARES_FRAME_LOG_ACK: {
        payload_len = FSIZEOF_FIELD(LOG_ACK.part) +
                      FSIZEOF_FIELD(LOG_ACK.num_parts) +
                      FSIZEOF_FIELD(LOG_ACK.id) + FSIZEOF_FIELD(LOG_ACK.log_id);
        break;
    }
    case ARES_FRAME_VERSION: {
        payload_len = FSIZEOF_FIELD(VERSION.app) + FSIZEOF_FIELD(VERSION.ncs) +
                      FSIZEOF_FIELD(VERSION.kernel);
        break;
    }
    case ARES_FRAME_DBG: {
        payload_len = FSIZEOF_FIELD(DBG);
        break;
    }
    default: {
        __ASSERT(false, "Invalid frame type received");
        break;
    }
    }

    return payload_len + ARES_FRAME_OVERHEAD;
}

#define Z_FSERIALIZE_LEN(field, len)                                           \
    do {                                                                       \
        (void)memcpy(payload, &frame.payload.field, (len));                    \
        payload += (len);                                                      \
    } while (0)
#define Z_FSERIALIZE_FIELD(field)                                              \
    do {                                                                       \
        (void)memcpy(payload, &frame->payload.field,                           \
                     SIZEOF_FIELD(struct ares_frame, payload.field));          \
        payload += SIZEOF_FIELD(struct ares_frame, payload.field);             \
    } while (0)

#define FSERIALIZE(field, len...)                                              \
    COND_CODE_0(IS_EMPTY(len), (Z_FSERIALIZE_LEN(field, len)),                 \
                (Z_FSERIALIZE_FIELD(field)))

#define FSERIALIZE_PTR(field, len)                                             \
    do {                                                                       \
        (void)memcpy(payload, frame->payload.field, (len));                    \
        payload += (len);                                                      \
    } while (0)

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
        FSERIALIZE(SETTING.setting);
        FSERIALIZE(SETTING.value);
        break;
    }
    case ARES_FRAME_START: {
        FSERIALIZE(START.sec);
        FSERIALIZE(START.ns);
        FSERIALIZE(START.id);
        FSERIALIZE(START.broadcast);
        FSERIALIZE(START.seq_cnt);
        FSERIALIZE(START.packet_id);
        break;
    }
    case ARES_FRAME_LOG: {
        FSERIALIZE(LOG.broadcast);
        FSERIALIZE(LOG.id);
        FSERIALIZE(LOG.tx_cnt);
        FSERIALIZE(LOG.part);
        FSERIALIZE(LOG.num_parts);
        FSERIALIZE(LOG.log_id);
        FSERIALIZE_PTR(LOG.msg, frame->payload.LOG.msg_len);
        break;
    }
    case ARES_FRAME_LOG_ACK: {
        FSERIALIZE(LOG_ACK.part);
        FSERIALIZE(LOG_ACK.num_parts);
        FSERIALIZE(LOG_ACK.id);
        FSERIALIZE(LOG_ACK.log_id);
        break;
    }
    case ARES_FRAME_ACK: {
        FSERIALIZE(ACK);
        break;
    }
    case ARES_FRAME_LED: {
        FSERIALIZE(LED);
        break;
    }
    case ARES_FRAME_HEARTBEAT: {
        FSERIALIZE(HEARTBEAT.flags);
        FSERIALIZE(HEARTBEAT.tx_count);
        FSERIALIZE(HEARTBEAT.id);
        break;
    }
    case ARES_FRAME_CLAIM: {
        FSERIALIZE(CLAIM);
        break;
    }
    case ARES_FRAME_FRAMING_ERROR: {
        uint32_t error_code = frame->payload.FRAMING_ERROR;
        (void)memcpy(payload, &error_code, sizeof(error_code));
        break;
    }
    case ARES_FRAME_VERSION: {
        FSERIALIZE(VERSION.app);
        FSERIALIZE(VERSION.ncs);
        FSERIALIZE(VERSION.kernel);
        break;
    }
    case ARES_FRAME_DBG: {
        FSERIALIZE(DBG);
        break;
    }
    default:
        // ARES_FRAME_LORA_CONFIG is RX only
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

#define FDESERIALIZE_INIT() size_t z_offset = 0

#define Z_FDESERIALIZE_FIELD(field)                                            \
    do {                                                                       \
        (void)memcpy(&frame->payload.field, &payload[z_offset],                \
                     SIZEOF_FIELD(struct ares_frame, payload.field));          \
        z_offset += SIZEOF_FIELD(struct ares_frame, payload.field);            \
    } while (0)

#define Z_FDESERIALIZE_LEN(field, len)                                         \
    do {                                                                       \
        (void)memcpy(&frame->payload.field, &payload[z_offset], (len));        \
        z_offset += (len);                                                     \
    } while (0)

#define FDESERIALIZE(field, len...)                                            \
    COND_CODE_0(IS_EMPTY(len), (Z_FDESERIALIZE_LEN(field, len)),               \
                (Z_FDESERIALIZE_FIELD(field)))

#define FDESERIALIZE_BUF(field, type_cast, len_field)                          \
    do {                                                                       \
        frame->payload.field = (type_cast)(payload + z_offset);                \
        frame->payload.len_field =                                             \
            payload_len - ((const uint8_t *)frame->payload.field - payload);   \
    } while (0)

static void deserialize(struct ares_frame *frame, const uint8_t *buf) {
    __ASSERT_NO_MSG(buf != NULL);
    const uint8_t *payload = &buf[ARES_FRAME_PAYLOAD_OFFSET];
    uint64_t payload_len = retrieve_payload_length(buf);

    frame->type = (enum ares_frame_type)buf[ARES_FRAME_TYPE_OFFSET];

    FDESERIALIZE_INIT();
    switch (frame->type) {
    case ARES_FRAME_SETTING: {
        FDESERIALIZE(SETTING.setting);
        if (payload_len == FSIZEOF_FIELD(SETTING.setting)) {
            frame->payload.SETTING.set = false;
        } else {
            frame->payload.SETTING.set = true;
            FDESERIALIZE(SETTING.value);
        }
        break;
    }
    case ARES_FRAME_START: {
        FDESERIALIZE(START.sec);
        FDESERIALIZE(START.ns);
        FDESERIALIZE(START.id);
        FDESERIALIZE(START.broadcast);
        // receive side: we don't care about the seq_cnt or packet_id...
        break;
    }
    case ARES_FRAME_LORA_CONFIG: {
        FDESERIALIZE(LORA_CONFIG, payload_len);
        break;
    }
    case ARES_FRAME_LED: {
        FDESERIALIZE(LED, payload_len);
        break;
    }
    case ARES_FRAME_HEARTBEAT: {
        FDESERIALIZE(HEARTBEAT.flags);
        FDESERIALIZE(HEARTBEAT.tx_count);
        FDESERIALIZE(HEARTBEAT.id);
        break;
    }
    case ARES_FRAME_CLAIM: {
        FDESERIALIZE(CLAIM);
        break;
    }
    case ARES_FRAME_LOG: {
        FDESERIALIZE(LOG.broadcast);
        FDESERIALIZE(LOG.id);
        FDESERIALIZE(LOG.tx_cnt);
        FDESERIALIZE(LOG.part);
        FDESERIALIZE(LOG.num_parts);
        FDESERIALIZE(LOG.log_id);
        FDESERIALIZE_BUF(LOG.msg, const char *, LOG.msg_len);
        break;
    }
    case ARES_FRAME_VERSION: {
        // nop: This is a request so it doesn't make sense to look at the
        // payload
        break;
    }
    default: {
        // ARES_FRAME_LOG_ACK, ARES_FRAME_ACK, ARES_FRAME_FRAMING_ERROR,
        // and ARES_FRAME_DBG are TX only
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
