/**
 * @file packet.h
 *
 * @brief Ares Packet API.
 *
 * @date 3/24/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_PACKET_H
#define ARES_PACKET_H

#include <zephyr/kernel.h>

/**
 * First byte in the header.
 */
#define ARES_PACKET_HEADER_0 'A'

/**
 * Second byte in the header.
 */
#define ARES_PACKET_HEADER_1 'R'

/**
 * First byte in the footer.
 */
#define ARES_PACKET_FOOTER_0 'E'

/**
 * Second byte in the footer.
 */
#define ARES_PACKET_FOOTER_1 'S'

/**
 * Number of bytes needed for the header field.
 */
#define ARES_PACKET_HEADER_OVERHEAD 2

/**
 * Number of bytes needed for the length field.
 */
#define ARES_PACKET_LEN_OVERHEAD 2

/**
 * Number of bytes needed for the type field.
 */
#define ARES_PACKET_TYPE_OVERHEAD 1

/**
 * Number of bytes needed for the packet ID field.
 */
#define ARES_PACKET_ID_OVERHEAD 2

/**
 * Number of bytes needed for the sequence count field.
 */
#define ARES_PACKET_SEQ_CNT_OVERHEAD 1

/**
 * Number of bytes needed for the PAN ID field.
 */
#define ARES_PACKET_PAN_ID_OVERHEAD 2

/**
 * Number of bytes needed for the source ID field.
 */
#define ARES_PACKET_SRC_ID_OVERHEAD 2

/**
 * Number of bytes needed for the destination ID field.
 */
#define ARES_PACKET_DST_ID_OVERHEAD 2

/**
 * Number of bytes needed for the payload type field.
 */
#define ARES_PACKET_PAYLOAD_TYPE_OVERHEAD 1

/**
 * Number of bytes needed for the CRC16 field.
 */
#define ARES_PACKET_CRC_OVERHEAD 2

/**
 * Number of bytes needed for the footer field.
 */
#define ARES_PACKET_FOOTER_OVERHEAD 2

/**
 * Number of bytes needed for a broadcast packet.
 */
#define ARES_PACKET_BROADCAST_OVERHEAD                                         \
    (ARES_PACKET_HEADER_OVERHEAD + ARES_PACKET_LEN_OVERHEAD +                  \
     ARES_PACKET_TYPE_OVERHEAD + ARES_PACKET_ID_OVERHEAD +                     \
     ARES_PACKET_SEQ_CNT_OVERHEAD + ARES_PACKET_PAN_ID_OVERHEAD +              \
     ARES_PACKET_SRC_ID_OVERHEAD + ARES_PACKET_PAYLOAD_TYPE_OVERHEAD +         \
     ARES_PACKET_CRC_OVERHEAD + ARES_PACKET_FOOTER_OVERHEAD)

/**
 * Number of bytes needed for a directed packet.
 */
#define ARES_PACKET_DIRECT_OVERHEAD                                            \
    (ARES_PACKET_BROADCAST_OVERHEAD + ARES_PACKET_DST_ID_OVERHEAD)

/**
 * @enum ares_packet_payload_type
 * @brief Packet payload types.
 */
enum ares_packet_payload_type {
    ARES_PKT_PAYLOAD_START = 0,     ///< Start data collection packet.
    ARES_PKT_PAYLOAD_HEARTBEAT = 1, ///< Heartbeat packet.
    ARES_PKT_PAYLOAD_CLAIM = 2,     ///< Claim master packet.
    ARES_PKT_PAYLOAD_LOG = 3,       ///< Log packet.
    ARES_PKT_PAYLOAD_LOG_ACK = 4,   ///< Log acknowledge packet.

    ARES_PKT_PAYLOAD_INVALID, ///< Invalid packet type.
};

/**
 * @struct ares_packet_payload
 * @brief Payload of ares packets.
 */
struct ares_packet_payload {
    /**
     * The payload type.
     */
    enum ares_packet_payload_type type;

    /**
     * The payload data.
     */
    union {
        struct {
            int64_t sec;
            uint64_t nsec;
        } START;

        struct {
            bool ready;
        } HEARTBEAT;

        struct {
            uint8_t part;
            uint8_t num_parts;
            uint16_t log_id;
            const char *msg;
            size_t msg_len;
        } LOG;

        struct {
            uint8_t part;
            uint8_t num_parts;
            uint16_t log_id;
        } LOG_ACK;
    } payload;
};

/**
 * @enum ares_packet_type
 * @brief Ares packet types.
 */
enum ares_packet_type {
    ARES_PKT_TYPE_BROADCAST = 0, ///< Broadcast message to all listening nodes.
    ARES_PKT_TYPE_DIRECT = 1,    ///< Direct a packet to a specific node.

    ARES_PKT_TYPE_INVALID, ///< Invalid packet type.
};

/**
 * @struct ares_packet
 * @brief Structured representation of an ares packet.
 */
struct ares_packet {
    /**
     * Sequence count value.
     */
    uint8_t sequence_cnt;

    /**
     * Personal Ares Network ID.
     */
    uint16_t pan_id;

    /**
     * Source ID of the packet.
     */
    uint16_t source_id;

    /**
     * Destination ID of the packet.
     * @note This field is ignored if the packet is a broadcast packet.
     */
    uint16_t destination_id;

    /**
     * Packet ID. Similar to sequence count except this one is for unique
     * transmissions.
     */
    uint16_t packet_id;

    /**
     * The type of packet received or to construct.
     */
    enum ares_packet_type type;

    /**
     * The packet payload.
     */
    struct ares_packet_payload payload;
};

/**
 * @struct ares_packet_info
 * @brief Serial buffer metadata that describes the location of a packet in a
 * buffer, the size of the packet, and how many bytes need to be read before
 * deserialization is possible.
 */
struct ares_packet_info {
    /**
     * Start index.
     *
     * @note -1 if no header packet found.
     */
    int start;

    /**
     * Size of the packet.
     *
     * @note -1 if no packet header found.
     */
    int size;

    /**
     * Number of additional bytes needed for a complete packet.
     *
     * @note -1 if no packet header found.
     */
    int bytes_left;
};

/**
 * @brief Function to serialize an ares packet into a buffer.
 *
 * @param[out] buf Pointer to the start of the buffer to write the packet to.
 * @param[in] len The size of the buffer.
 * @param[in] packet Pointer to the structured packet to serialize.
 * @param[in] seq_num The sequence number for the packet.
 *
 * @return The number of bytes written to the buffer on success.
 * @return -EINVAL on invalid parameters.
 * @return -EBADMSG if the packet is invalid.
 * @return -ENOBUFS if the buffer is too small to contain the packet.
 */
int serialize_ares_packet(uint8_t *buf, size_t len,
                          const struct ares_packet *packet, uint8_t seq_num);

/**
 * @brief Function to deserialize an ares packet from a buffer.
 *
 * @param[out] packet Pointer to the structured packet to store the results in.
 * @param[in] buf Pointer to the start of the packet in the buffer.
 * @param[in] len The length of the packet.
 *
 * @return 0 on success.
 * @return -EINVAL on invalid parameters or packet not valid.
 */
int deserialize_ares_packet(struct ares_packet *packet, const uint8_t *buf,
                            size_t len);

/**
 * @brief Function to verify the packet in the buffer.
 *
 * @param[in] buf Pointer to the start of the packet in the buffer.
 * @param[in] len The length of the packet in the buffer.
 *
 * @return `true` if the packet is valid.
 * @return `false` if the inputs or the packet are invalid.
 */
bool ares_packet_valid(const uint8_t *buf, size_t len);

/**
 * @brief Function to check if there is an ares packet in the buffer.
 *
 * @param[in] buf Pointer to the beginning of the buffer.
 * @param[in] len The number of bytes in the buffer.
 * @param[out] info The metadata for finding the packet in the buffer.
 *
 * @return 1 if complete packet was found.
 * @return 0 if complete packet was not found.
 * @return -EINVAL if parameters are invalid.
 */
int ares_packet_present(const uint8_t *buf, size_t len,
                        struct ares_packet_info *info);

#endif // ARES_PACKET_H