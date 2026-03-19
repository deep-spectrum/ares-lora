/**
 * @file lora_frame.c
 *
 * @brief
 *
 * @date 3/19/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <lora_frame.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/crc.h>

#define LORA_FRAME_SEND_ID_OVERHEAD 8
#define LORA_FRAME_SEQ_CNT_OVERHEAD 1
#define LORA_FRAME_SECONDS_OVERHEAD 8
#define LORA_FRAME_NS_OVERHEAD 8
#define LORA_FRAME_CRC_OVERHEAD 2

#define LORA_FRAME_MSG_OVERHEAD (LORA_FRAME_SEND_ID_OVERHEAD + LORA_FRAME_SEQ_CNT_OVERHEAD + LORA_FRAME_SECONDS_OVERHEAD + LORA_FRAME_NS_OVERHEAD)
#define LORA_FRAME_OVERHEAD (LORA_FRAME_MSG_OVERHEAD + LORA_FRAME_CRC_OVERHEAD)

#define LORA_FRAME_SEND_ID_OFFSET 0
#define LORA_FRAME_SEQ_CNT_OFFSET (LORA_FRAME_SEND_ID_OFFSET + LORA_FRAME_SEND_ID_OVERHEAD)
#define LORA_FRAME_SECONDS_OFFSET (LORA_FRAME_SEQ_CNT_OFFSET + LORA_FRAME_SEQ_CNT_OVERHEAD)
#define LORA_FRAME_NS_OFFSET (LORA_FRAME_SECONDS_OFFSET + LORA_FRAME_SECONDS_OVERHEAD)
#define LORA_FRAME_CRC_OFFSET (LORA_FRAME_NS_OFFSET + LORA_FRAME_NS_OVERHEAD)

const size_t lora_frame_size = LORA_FRAME_OVERHEAD;

static void serialize_field(uint8_t *buf, const uint64_t field, size_t field_size) {
    size_t i = 0;
    for (; i < field_size; i++) {
        buf[i] = (field >> (i * CHAR_BIT)) & UINT8_MAX;
    }
}

#define SERIALIZE(buf_, field_) serialize_field(buf_, field_, sizeof(field_))

static void deserialize_field(const uint8_t *buf, void *field, size_t field_size) {
    uint8_t *field_ = field;
    for (size_t i = 0; i < field_size; i++) {
        field_[i] = buf[i];
    }
}

#define DESERIALIZE(buf_, field_) deserialize_field(buf_, &field_, sizeof(field_))

#define REVERSE_2(x) ((((x) & 1) << 1) | (((x) >> 1) & 1))
#define REVERSE_4(x) ((REVERSE_2(x) << 2) | (REVERSE_2(x) >> 2))
#define REVERSE_8(x) ((REVERSE_4(x) << 4) | (REVERSE_4(x) >> 4))
#define REVERSE_16(x) ((REVERSE_8(x) << 8) | (REVERSE_8(x) >> 8))

static void reflect(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        buf[i] = REVERSE_8(buf[i]);
    }
}

static uint16_t compute_crc(const uint8_t *buf, size_t len) {
    uint16_t ret;

#if !IS_ENABLED(CONFIG_LORA_REFLECT_INPUT)
    uint8_t reflected_data[LORA_FRAME_MSG_OVERHEAD];
    (void)memcpy(reflected_data, buf, len);
    reflect(reflected_data, len);
    ret = crc16(CONFIG_LORA_CRC_POLYNOMIAL, CONFIG_LORA_CRC_SEED, reflected_data, len);
#else
    ret = crc16(CONFIG_LORA_CRC_POLYNOMIAL, CONFIG_LORA_CRC_SEED, buf, len);
#endif // !IS_ENABLED(CONFIG_LORA_REFLECT_INPUT)

#if !IS_ENABLED(CONFIG_LORA_REFLECT_CRC_OUTPUT)
    ret = REVERSE_16(ret);
#endif // !IS_ENABLED(CONFIG_LORA_REFLECT_CRC_OUTPUT)

    return ret ^ CONFIG_LORA_CRC_XOR_OUTPUT;
}

int serialize_lora_frame(uint8_t *buf, size_t len, const struct lora_frame *frame) {
    uint16_t crc;

    if (len < lora_frame_size) {
        return -ENOBUFS;
    }

    if (buf == NULL || frame == NULL) {
        return -EINVAL;
    }

    SERIALIZE(buf + LORA_FRAME_SEND_ID_OFFSET, frame->id);
    SERIALIZE(buf + LORA_FRAME_SEQ_CNT_OFFSET, frame->seq_cnt);
    SERIALIZE(buf + LORA_FRAME_SECONDS_OFFSET, frame->second);
    SERIALIZE(buf + LORA_FRAME_NS_OFFSET, frame->nanosecond);

    crc = compute_crc(buf, LORA_FRAME_MSG_OVERHEAD);

    SERIALIZE(buf + LORA_FRAME_CRC_OFFSET, crc);

    return 0;
}

enum lora_frame_check_result check_lora_frame(const uint8_t *buf, size_t len) {
    uint16_t rx_crc, crc;

    if (len < lora_frame_size || buf == NULL) {
        return LORA_FRAME_INVALID;
    }

    DESERIALIZE(buf + LORA_FRAME_CRC_OFFSET, rx_crc);
    crc = compute_crc(buf, LORA_FRAME_MSG_OVERHEAD);

    return (rx_crc == crc) ? LORA_FRAME_CHECK_OK : LORA_FRAME_BAD_CRC;
}

int deserialize_lora_frame(const uint8_t *buf, size_t len, struct lora_frame *frame) {
    enum lora_frame_check_result check;
    if (len < lora_frame_size || buf == NULL || frame == NULL) {
        return -EINVAL;
    }

    check = check_lora_frame(buf, len);
    if (check != LORA_FRAME_CHECK_OK) {
        return -EBADMSG;
    }

    DESERIALIZE(buf + LORA_FRAME_SEND_ID_OFFSET, frame->id);
    DESERIALIZE(buf + LORA_FRAME_SEQ_CNT_OFFSET, frame->seq_cnt);
    DESERIALIZE(buf + LORA_FRAME_SECONDS_OFFSET, frame->second);
    DESERIALIZE(buf + LORA_FRAME_NS_OFFSET, frame->nanosecond);

    return 0;
}
