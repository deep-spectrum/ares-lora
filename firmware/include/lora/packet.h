/**
 * @file packet.h
 *
 * @brief
 *
 * @date 3/24/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_PACKET_H
#define ARES_PACKET_H

#include <zephyr/kernel.h>

#define ARES_PACKET_HEADER_0              'A'
#define ARES_PACKET_HEADER_1              'R'
#define ARES_PACKET_FOOTER_0              'E'
#define ARES_PACKET_FOOTER_1              'S'

#define ARES_PACKET_HEADER_OVERHEAD       2
#define ARES_PACKET_LEN_OVERHEAD          2
#define ARES_PACKET_TYPE_OVERHEAD         1
#define ARES_PACKET_ID_OVERHEAD           2
#define ARES_PACKET_SEQ_CNT_OVERHEAD      1
#define ARES_PACKET_PAN_ID_OVERHEAD       2
#define ARES_PACKET_SRC_ID_OVERHEAD       2
#define ARES_PACKET_DST_ID_OVERHEAD       2
#define ARES_PACKET_PAYLOAD_TYPE_OVERHEAD 1
#define ARES_PACKET_CRC_OVERHEAD          2
#define ARES_PACKET_FOOTER_OVERHEAD       2

#define ARES_PACKET_BROADCAST_OVERHEAD                                         \
    (ARES_PACKET_HEADER_OVERHEAD + ARES_PACKET_LEN_OVERHEAD +                  \
     ARES_PACKET_TYPE_OVERHEAD + ARES_PACKET_ID_OVERHEAD +                     \
     ARES_PACKET_SEQ_CNT_OVERHEAD + ARES_PACKET_PAN_ID_OVERHEAD +              \
     ARES_PACKET_SRC_ID_OVERHEAD + ARES_PACKET_PAYLOAD_TYPE_OVERHEAD +         \
     ARES_PACKET_CRC_OVERHEAD + ARES_PACKET_FOOTER_OVERHEAD)
#define ARES_PACKET_DIRECT_OVERHEAD                                            \
    (ARES_PACKET_BROADCAST_OVERHEAD + ARES_PACKET_DST_ID_OVERHEAD)

enum ares_packet_payload_type {
    ARES_PKT_PAYLOAD_START = 0,
    ARES_PKT_PAYLOAD_HEARTBEAT = 1,

    ARES_PKT_PAYLOAD_INVALID,
};

struct ares_packet_payload {
    enum ares_packet_payload_type type;
    union {
        struct {
            int64_t sec;
            uint64_t nsec;
        } START;

        struct {
            bool ready;
        } HEARTBEAT;
    } payload;
};

enum ares_packet_type {
    ARES_PKT_TYPE_BROADCAST = 0,
    ARES_PKT_TYPE_DIRECT = 1,

    ARES_PKT_TYPE_INVALID,
};

struct ares_packet {
    uint8_t sequence_cnt;
    uint16_t pan_id;
    uint16_t source_id;
    uint16_t destination_id;
    uint16_t packet_id;
    enum ares_packet_type type;
    struct ares_packet_payload payload;
};

struct ares_packet_info {
    int start;
    int size;
    int bytes_left;
};

int serialize_ares_packet(uint8_t *buf, size_t len,
                          const struct ares_packet *packet, uint8_t seq_num);
int deserialize_ares_packet(struct ares_packet *packet, const uint8_t *buf,
                            size_t len);
bool ares_packet_valid(const uint8_t *buf, size_t len);
int ares_packet_present(const uint8_t *buf, size_t len,
                        struct ares_packet_info *info);

#endif // ARES_PACKET_H