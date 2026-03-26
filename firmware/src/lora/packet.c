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

static void serialize_broadcast(uint8_t *buf, size_t len,
                                const struct ares_packet *packet) {}

static void serialize_direct(uint8_t *buf, size_t len,
                             const struct ares_packet *packet) {}

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
        serialize_broadcast(buf, len, packet);
        break;
    }
    case ARES_PKT_TYPE_DIRECT: {
        serialize_direct(buf, len, packet);
        break;
    }
    default: {
        return -EBADMSG;
    }
    }

    return (int)packet_len;
}
