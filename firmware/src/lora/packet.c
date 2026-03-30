/**
 * @file packet.c
 *
 * @brief
 *
 * @date 3/24/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <lora/packet.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/crc.h>

typedef uint16_t crc16_t;

#define ARES_PACKET_HEADER_OFFSET 0
#define ARES_PACKET_LEN_OFFSET                                                 \
    (ARES_PACKET_HEADER_OFFSET + ARES_PACKET_HEADER_OVERHEAD)
#define ARES_PACKET_TYPE_OFFSET                                                \
    (ARES_PACKET_LEN_OFFSET + ARES_PACKET_LEN_OVERHEAD)
#define ARES_PACKET_SEQ_CNT_OFFSET                                             \
    (ARES_PACKET_TYPE_OFFSET + ARES_PACKET_TYPE_OVERHEAD)
#define ARES_PACKET_PAN_ID_OFFSET                                              \
    (ARES_PACKET_SEQ_CNT_OFFSET + ARES_PACKET_SEQ_CNT_OVERHEAD)
#define ARES_PACKET_SRC_ID_OFFSET                                              \
    (ARES_PACKET_PAN_ID_OFFSET + ARES_PACKET_PAN_ID_OVERHEAD)
#define ARES_PACKET_DST_ID_OFFSET                                              \
    (ARES_PACKET_SRC_ID_OFFSET + ARES_PACKET_SRC_ID_OVERHEAD)
#define ARES_PACKET_PAYLOAD_TYPE_OFFSET(packet_type)                           \
    ((ARES_PACKET_SRC_ID_OFFSET + ARES_PACKET_SRC_ID_OVERHEAD) +               \
     ((packet_type == ARES_PKT_TYPE_DIRECT) * ARES_PACKET_DST_ID_OVERHEAD))
#define ARES_PACKET_PAYLOAD_OFFSET(packet_type)                                \
    (ARES_PACKET_PAYLOAD_TYPE_OFFSET(packet_type) +                            \
     ARES_PACKET_PAYLOAD_TYPE_OVERHEAD)
#define ARES_PACKET_CRC_OFFSET(packet_type, payload_len)                       \
    (ARES_PACKET_PAYLOAD_OFFSET(packet_type) + (payload_len))
#define ARES_PACKET_FOOTER_OFFSET(packet_type, payload_len)                    \
    (ARES_PACKET_CRC_OFFSET(packet_type, payload_len) +                        \
     ARES_PACKET_CRC_OVERHEAD)

#define ARES_PACKET_MIN_OVERHEAD                                               \
    MIN(ARES_PACKET_BROADCAST_OVERHEAD, ARES_PACKET_DIRECT_OVERHEAD)
#define ARES_PACKET_MAX_OVERHEAD                                               \
    MAX(ARES_PACKET_BROADCAST_OVERHEAD, ARES_PACKET_DIRECT_OVERHEAD)
#define ARES_PACKET_OVERHEAD(packet_type)                                      \
    ((packet_type == ARES_PKT_TYPE_BROADCAST)                                  \
         ? (ARES_PACKET_BROADCAST_OVERHEAD)                                    \
         : (ARES_PACKET_DIRECT_OVERHEAD))

static const uint16_t header = (uint16_t)ARES_PACKET_HEADER_0 |
                               ((uint16_t)ARES_PACKET_HEADER_1 << CHAR_BIT);
static const uint16_t footer = (uint16_t)ARES_PACKET_FOOTER_0 |
                               ((uint16_t)ARES_PACKET_FOOTER_1 << CHAR_BIT);

BUILD_ASSERT(
    sizeof(header) == ARES_PACKET_HEADER_OVERHEAD,
    "Mismatch between specified header overhead and the actual header type");
BUILD_ASSERT(
    sizeof(footer) == ARES_PACKET_FOOTER_OVERHEAD,
    "Mismatch between specified footer overhead and the actual footer type");

#if IS_ENABLED(CONFIG_LORA_REFLECT_CRC_OUTPUT)
static uint8_t reverse_byte(uint8_t byte) {
    uint8_t n0 = byte & 0xF;
    uint8_t n1 = (byte >> 4) & 0xF;

    uint8_t h0n0 = n0 & 0x3;
    uint8_t h1n0 = (n0 >> 2) & 0x3;
    uint8_t h0n1 = n1 & 0x3;
    uint8_t h1n1 = (n1 >> 2) & 0x3;

    h0n0 = ((h0n0 & 0x1) << 1) | ((h0n0 >> 1) & 0x1);
    h1n0 = ((h1n0 & 0x1) << 1) | ((h1n0 >> 1) & 0x1);

    n0 = (h0n0 << 2) | h1n0;

    h0n1 = ((h0n1 & 0x1) << 1) | ((h0n1 >> 1) & 0x1);
    h1n1 = ((h1n1 & 0x1) << 1) | ((h1n1 >> 1) & 0x1);

    n1 = (h0n1 << 2) | h1n1;

    return (n0 << 4) | n1;
}

static void reflect(void *data, size_t size) {
    __ASSERT_NO_MSG(data);
    __ASSERT_NO_MSG(size);
    uint8_t *buf = data;

    if (size == 1) {
        reverse_byte(*buf);
        return;
    }

    for (size_t i = 0, j = (size - 1); i <= j; i++, j--) {
        uint8_t end = buf[j];
        uint8_t start = buf[i];

        end = reverse_byte(end);
        start = reverse_byte(start);

        buf[i] = end;
        buf[j] = start;
    }
}
#endif // IS_ENABLED(CONFIG_LORA_REFLECT_CRC_OUTPUT)

static size_t calculate_packet_size(const struct ares_packet *packet) {
    __ASSERT_NO_MSG(packet != NULL);
    size_t overhead = (packet->type == ARES_PKT_TYPE_BROADCAST)
                          ? ARES_PACKET_BROADCAST_OVERHEAD
                          : ARES_PACKET_DIRECT_OVERHEAD;

    switch (packet->payload.type) {
    case ARES_PKT_PAYLOAD_START: {
        overhead += sizeof(packet->payload.payload.timespec);
        break;
    }
    default: {
        return 0;
    }
    }

    return overhead;
}

static crc16_t compute_crc(const uint8_t *buf, size_t len) {
    crc16_t crc;
    crc = crc16(CONFIG_LORA_CRC_POLYNOMIAL, CONFIG_LORA_CRC_SEED, buf, len);

#if IS_ENABLED(CONFIG_LORA_REFLECT_CRC_OUTPUT)
    reflect(&crc, sizeof(crc));
#endif // !IS_ENABLED(CONFIG_LORA_REFLECT_CRC_OUTPUT)

    return crc ^ CONFIG_LORA_CRC_XOR_OUTPUT;
}

static void serialize(uint8_t *buf, size_t len,
                      const struct ares_packet *packet, size_t packet_length) {
    __ASSERT_NO_MSG(buf != NULL);
    __ASSERT_NO_MSG(packet != NULL);
    crc16_t crc;
    size_t payload_len = packet_length - ARES_PACKET_BROADCAST_OVERHEAD;
    uint8_t *payload = &buf[ARES_PACKET_PAYLOAD_OFFSET(1)];

    uint16_t su_payload_len = (uint16_t)payload_len;

    __ASSERT_NO_MSG(payload_len < (size_t)UINT16_MAX);
    memset(buf, 0, len);

    (void)memcpy(&buf[ARES_PACKET_HEADER_OFFSET], &header,
                 ARES_PACKET_HEADER_OVERHEAD);
    (void)memcpy(&buf[ARES_PACKET_LEN_OFFSET], &su_payload_len,
                 ARES_PACKET_LEN_OVERHEAD);
    buf[ARES_PACKET_TYPE_OFFSET] = packet->type;
    buf[ARES_PACKET_SEQ_CNT_OFFSET] = packet->sequence_cnt;
    (void)memcpy(&buf[ARES_PACKET_PAN_ID_OFFSET], &packet->pan_id,
                 ARES_PACKET_PAN_ID_OVERHEAD);
    (void)memcpy(&buf[ARES_PACKET_SRC_ID_OFFSET], &packet->source_id,
                 ARES_PACKET_SRC_ID_OVERHEAD);

    if (packet->type == ARES_PKT_TYPE_DIRECT) {
        (void)memcpy(&buf[ARES_PACKET_HEADER_OFFSET], &packet->destination_id,
                     ARES_PACKET_DST_ID_OVERHEAD);
    }

    buf[ARES_PACKET_PAYLOAD_TYPE_OFFSET(packet->type)] = packet->payload.type;

    switch (packet->payload.type) {
    case ARES_PKT_PAYLOAD_START: {
        (void)memcpy(payload, &packet->payload.payload.timespec, payload_len);
        break;
    }
    default: {
        __ASSERT_NO_MSG(false);
    }
    }

    crc = compute_crc(buf, ARES_PACKET_CRC_OFFSET(packet->type, payload_len));
    (void)memcpy(&buf[ARES_PACKET_CRC_OFFSET(packet->type, payload_len)], &crc,
                 ARES_PACKET_CRC_OVERHEAD);
    (void)memcpy(&buf[ARES_PACKET_FOOTER_OFFSET(packet->type, payload_len)],
                 &footer, ARES_PACKET_FOOTER_OVERHEAD);
}

int serialize_ares_packet(uint8_t *buf, size_t len,
                          const struct ares_packet *packet) {
    size_t packet_len = 0;

    if (buf == NULL || packet == NULL) {
        return -EINVAL;
    }

    packet_len = calculate_packet_size(packet);
    if (packet_len < (size_t)ARES_PACKET_MIN_OVERHEAD) {
        return -EBADMSG;
    }

    if (len < packet_len) {
        return -ENOBUFS;
    }

    serialize(buf, len, packet, packet_len);

    return (int)packet_len;
}

static void deserialize(struct ares_packet *packet, const uint8_t *buf) {
    size_t payload_len;
    const uint8_t *payload;

    (void)memcpy(&payload_len, &buf[ARES_PACKET_LEN_OFFSET],
                 ARES_PACKET_LEN_OVERHEAD);
    packet->type = buf[ARES_PACKET_TYPE_OFFSET];
    packet->sequence_cnt = buf[ARES_PACKET_SEQ_CNT_OFFSET];
    (void)memcpy(&packet->pan_id, &buf[ARES_PACKET_PAN_ID_OFFSET],
                 ARES_PACKET_PAN_ID_OVERHEAD);
    (void)memcpy(&packet->source_id, &buf[ARES_PACKET_SRC_ID_OFFSET],
                 ARES_PACKET_SRC_ID_OVERHEAD);

    if (packet->type == ARES_PKT_TYPE_DIRECT) {
        (void)memcpy(&packet->destination_id, &buf[ARES_PACKET_DST_ID_OFFSET],
                     ARES_PACKET_DST_ID_OVERHEAD);
    }

    payload = &buf[ARES_PACKET_PAYLOAD_OFFSET(packet->type)];
    packet->payload.type = buf[ARES_PACKET_PAYLOAD_TYPE_OFFSET(packet->type)];

    switch (packet->payload.type) {
    case ARES_PKT_PAYLOAD_START: {
        (void)memcpy(&packet->payload.payload.timespec, payload, payload_len);
        break;
    }
    default: {
        __ASSERT_NO_MSG(false);
    }
    }
}

int deserialize_ares_packet(struct ares_packet *packet, const uint8_t *buf,
                            size_t len) {
    if (!ares_packet_valid(buf, len)) {
        return -EINVAL;
    }

    __ASSERT_NO_MSG(buf != NULL);
    if (packet == NULL) {
        return -EINVAL;
    }

    deserialize(packet, buf);

    return 0;
}

bool ares_packet_valid(const uint8_t *buf, size_t len) {
    enum ares_packet_type type;
    enum ares_packet_payload_type ptype;
    size_t payload_len;
    crc16_t crc;

    if (buf == NULL || len < ARES_PACKET_MIN_OVERHEAD) {
        return false;
    }

    if (memcmp(&buf[ARES_PACKET_HEADER_OFFSET], &header,
               ARES_PACKET_HEADER_OVERHEAD) != 0) {
        // invalid header
        return false;
    }

    type = buf[ARES_PACKET_TYPE_OFFSET];
    if (type >= ARES_PKT_TYPE_INVALID) {
        // invalid type
        return false;
    }

    ptype = buf[ARES_PACKET_PAYLOAD_TYPE_OFFSET(type)];
    if (ptype >= ARES_PKT_PAYLOAD_INVALID) {
        // invalid packet type
        return false;
    }

    (void)memcpy(&payload_len, &buf[ARES_PACKET_LEN_OFFSET],
                 ARES_PACKET_LEN_OVERHEAD);
    if ((payload_len + ARES_PACKET_OVERHEAD(type)) > len) {
        // invalid length (avoid accessing memory we are not supposed to access)
        return false;
    }

    if (memcmp(&buf[ARES_PACKET_FOOTER_OFFSET(type, payload_len)], &footer,
               ARES_PACKET_FOOTER_OVERHEAD) != 0) {
        // invalid footer
        return false;
    }

    crc = compute_crc(buf, ARES_PACKET_CRC_OFFSET(type, payload_len));
    if (memcmp(&crc, &buf[ARES_PACKET_CRC_OFFSET(type, payload_len)],
               ARES_PACKET_CRC_OVERHEAD) != 0) {
        // invalid crc
        return false;
    }

    return true;
}

int ares_packet_present(const uint8_t *buf, size_t len,
                        struct ares_packet_info *info) {
    int *start, *length, *remaining;
    size_t seq_cnt_index, payload_len, footer_index;
    enum ares_packet_type type;

    if (buf == NULL || info == NULL) {
        return -EINVAL;
    }

    start = &info->start;
    length = &info->size;
    remaining = &info->bytes_left;

    for (size_t i = 0; i < len; i++) {
        if (buf[i] != ARES_PACKET_HEADER_0 || (i + 1) >= len ||
            buf[i + 1] != ARES_PACKET_HEADER_1) {
            continue;
        }

        *start = (int)i;

        seq_cnt_index = *start + ARES_PACKET_SEQ_CNT_OFFSET;
        if (seq_cnt_index > len) {
            // cannot extract packet length
            continue;
        }

        (void)memcpy(&payload_len, &buf[ARES_PACKET_LEN_OFFSET],
                     ARES_PACKET_LEN_OVERHEAD);
        type = buf[ARES_PACKET_TYPE_OFFSET];

        footer_index = *start + ARES_PACKET_FOOTER_OFFSET(type, payload_len);
        if (footer_index >= len) {
            *remaining = (int)footer_index - ((int)len - *start) + 1;
            continue;
        }

        if ((footer_index + 1) >= len) {
            *remaining = 1;
            continue;
        }

        if (memcmp(&buf[footer_index], &footer, ARES_PACKET_FOOTER_OVERHEAD) !=
            0) {
            // Not a packet
            continue;
        }

        *length = (int)payload_len + ARES_PACKET_OVERHEAD(type);
        *remaining = 0;

        return 1;
    }

    return 0;
}
