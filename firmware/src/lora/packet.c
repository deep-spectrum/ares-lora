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
#define ARES_PACKET_PAYLOAD_TYPE_OFFSET(broadcast)                             \
    COND_CODE_1(IS_EQ(broadcast, 1),                                           \
                ((ARES_PACKET_SRC_ID_OFFSET + ARES_PACKET_SRC_ID_OVERHEAD)),   \
                ((ARES_PACKET_DST_ID_OFFSET + ARES_PACKET_DST_ID_OVERHEAD)))
#define ARES_PACKET_PAYLOAD_OFFSET(broadcast)                                  \
    (ARES_PACKET_PAYLOAD_TYPE_OFFSET(broadcast) +                              \
     ARES_PACKET_PAYLOAD_TYPE_OVERHEAD)
#define ARES_PACKET_CRC_OFFSET(broadcast, payload_len)                         \
    (ARES_PACKET_PAYLOAD_OFFSET(broadcast) + (payload_len))
#define ARES_PACKET_FOOTER_OFFSET(broadcast, payload_len)                      \
    (ARES_PACKET_CRC_OFFSET(broadcast, payload_len) + ARES_PACKET_CRC_OVERHEAD)

#define ARES_PACKET_MIN_OVERHEAD                                               \
    MIN(ARES_PACKET_BROADCAST_OVERHEAD, ARES_PACKET_DIRECT_OVERHEAD)
#define ARES_PACKET_MAX_OVERHEAD                                               \
    MAX(ARES_PACKET_BROADCAST_OVERHEAD, ARES_PACKET_DIRECT_OVERHEAD)

#define REVERSE_2(x)  ((((x)&1) << 1) | (((x) >> 1) & 1))
#define REVERSE_4(x)  ((REVERSE_2(x) << 2) | (REVERSE_2(x) >> 2))
#define REVERSE_8(x)  ((REVERSE_4(x) << 4) | (REVERSE_4(x) >> 4))
#define REVERSE_16(x) ((REVERSE_8(x) << 8) | (REVERSE_8(x) >> 8))

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

static void serialize_header(uint8_t *buf, size_t len,
                             const struct ares_packet *packet,
                             size_t payload_len) {
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
}

static void serialize_payload(uint8_t *payload, size_t payload_len,
                              const struct ares_packet *packet) {
    switch (packet->payload.type) {
    case ARES_PKT_PAYLOAD_START: {
        (void)memcpy(payload, &packet->payload.payload.timespec, payload_len);
        break;
    }
    default: {
        __ASSERT_NO_MSG(false);
    }
    }
}

static crc16_t compute_crc(const uint8_t *buf, size_t len) {
    crc16_t crc;
    crc = crc16(CONFIG_LORA_CRC_POLYNOMIAL, CONFIG_LORA_CRC_SEED, buf, len);

#if !IS_ENABLED(CONFIG_LORA_REFLECT_CRC_OUTPUT)
    crc = REVERSE_16(crc);
#endif // !IS_ENABLED(CONFIG_LORA_REFLECT_CRC_OUTPUT)

    return crc ^ CONFIG_LORA_CRC_XOR_OUTPUT;
}

static void serialize_footer(uint8_t *buf, size_t crc_offset,
                             size_t footer_offset) {
    // crc offset is also the number of bytes written to the buffer...
    crc16_t crc = compute_crc(buf, crc_offset);

    (void)memcpy(&buf[crc_offset], &crc, ARES_PACKET_CRC_OVERHEAD);
    (void)memcpy(&buf[footer_offset], &footer, ARES_PACKET_FOOTER_OVERHEAD);
}

static void serialize_broadcast(uint8_t *buf, size_t len,
                                const struct ares_packet *packet,
                                size_t packet_length) {
    __ASSERT_NO_MSG(buf != NULL);
    __ASSERT_NO_MSG(packet != NULL);
    size_t payload_len = packet_length - ARES_PACKET_BROADCAST_OVERHEAD;
    uint8_t *payload = &buf[ARES_PACKET_PAYLOAD_OFFSET(1)];

    serialize_header(buf, len, packet, payload_len);
    buf[ARES_PACKET_PAYLOAD_TYPE_OFFSET(1)] = packet->payload.type;

    serialize_payload(payload, payload_len, packet);
    serialize_footer(buf, ARES_PACKET_CRC_OFFSET(1, payload_len),
                     ARES_PACKET_FOOTER_OFFSET(1, payload_len));
}

static void serialize_direct(uint8_t *buf, size_t len,
                             const struct ares_packet *packet,
                             size_t packet_length) {
    __ASSERT_NO_MSG(buf != NULL);
    __ASSERT_NO_MSG(packet != NULL);
    size_t payload_len = packet_length - ARES_PACKET_DIRECT_OVERHEAD;
    uint8_t *payload = &buf[ARES_PACKET_PAYLOAD_OFFSET(0)];

    serialize_header(buf, len, packet, payload_len);
    (void)memcpy(&buf[ARES_PACKET_DST_ID_OFFSET], &packet->destination_id,
                 ARES_PACKET_DST_ID_OVERHEAD);
    buf[ARES_PACKET_PAYLOAD_TYPE_OFFSET(0)] = packet->payload.type;

    serialize_payload(payload, payload_len, packet);
    serialize_footer(buf, ARES_PACKET_CRC_OFFSET(0, payload_len),
                     ARES_PACKET_FOOTER_OFFSET(0, payload_len));
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

    switch (packet->type) {
    case ARES_PKT_TYPE_BROADCAST: {
        serialize_broadcast(buf, len, packet, packet_len);
        break;
    }
    case ARES_PKT_TYPE_DIRECT: {
        serialize_direct(buf, len, packet, packet_len);
        break;
    }
    default: {
        return -EBADMSG;
    }
    }

    return (int)packet_len;
}

static size_t deserialize(struct ares_packet *packet, const uint8_t *buf,
                          size_t len) {
    size_t payload_len;

    (void)memcpy(&payload_len, &buf[ARES_PACKET_LEN_OFFSET],
                 ARES_PACKET_LEN_OVERHEAD);
    packet->sequence_cnt = buf[ARES_PACKET_SEQ_CNT_OFFSET];
    (void)memcpy(&packet->pan_id, &buf[ARES_PACKET_PAN_ID_OFFSET],
                 ARES_PACKET_PAN_ID_OVERHEAD);
    (void)memcpy(&packet->source_id, &buf[ARES_PACKET_SRC_ID_OFFSET],
                 ARES_PACKET_SRC_ID_OVERHEAD);

    return payload_len;
}

static void deserialize_payload(struct ares_packet *packet,
                                const uint8_t *payload, size_t payload_len) {
    switch (packet->payload.type) {
    case ARES_PKT_PAYLOAD_START: {
        (void)memcpy(&packet->payload.payload.timespec, payload, payload_len);
        break;
    }
    }
}

static void deserialize_broadcast(struct ares_packet *packet,
                                  const uint8_t *buf, size_t len) {
    size_t payload_len = deserialize(packet, buf, len);
    const uint8_t *payload = &buf[ARES_PACKET_PAYLOAD_OFFSET(1)];

    packet->payload.type = buf[ARES_PACKET_PAYLOAD_TYPE_OFFSET(1)];

    deserialize_payload(packet, payload, payload_len);
}

static void deserialize_direct(struct ares_packet *packet, const uint8_t *buf,
                               size_t len) {
    size_t payload_len = deserialize(packet, buf, len);
    const uint8_t *payload = &buf[ARES_PACKET_PAYLOAD_OFFSET(0)];

    (void)memcpy(&packet->destination_id, &buf[ARES_PACKET_DST_ID_OFFSET],
                 ARES_PACKET_DST_ID_OVERHEAD);
    packet->payload.type = buf[ARES_PACKET_PAYLOAD_TYPE_OFFSET(0)];

    deserialize_payload(packet, payload, payload_len);
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

    packet->type = buf[ARES_PACKET_TYPE_OFFSET];
    switch (packet->type) {
    case ARES_PKT_TYPE_BROADCAST: {
        deserialize_broadcast(packet, buf, len);
        break;
    }
    case ARES_PKT_TYPE_DIRECT: {
        deserialize_direct(packet, buf, len);
        break;
    }
    default: {
        return -EBADMSG;
    }
    }

    return 0;
}
