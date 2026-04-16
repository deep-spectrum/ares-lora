/**
 * @file frame.h
 *
 * @brief
 *
 * | Field | Header | Length  | Type   | Payload       | Footer |
 * | Size  | 1 byte | 2 bytes | 1 byte | 0-65535 bytes | 1 byte |
 * | Value | ^      |         |        |               | @      |
 *
 * @date 3/19/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_SERIAL_FRAME_H
#define ARES_SERIAL_FRAME_H

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
    ARES_FRAME_SETTING,       ///< SETTING frame.
    ARES_FRAME_START,         ///< Start time frame.
    ARES_FRAME_LORA_CONFIG,   ///< LoRa configuration frame.
    ARES_FRAME_LED,           ///< Control LED state.
    ARES_FRAME_HEARTBEAT,     ///< LoRa Heart Beat frame.
    ARES_FRAME_CLAIM,         ///< LoRa host claim frame.
    ARES_FRAME_LOG,           ///< Log message.
    ARES_FRAME_ACK,           ///< ACK frame.
    ARES_FRAME_FRAMING_ERROR, ///< Framing error frame. TX only.

    ARES_FRAME_TYPE_INVALID,
};

struct ares_frame {
    enum ares_frame_type type;
    union {
        struct {
            uint16_t setting;
            uint32_t value;
            bool set;
        } SETTING; ///< ARES_FRAME_SETTING

        struct {
            int64_t sec;
            uint64_t ns;
            uint16_t id;
            uint16_t packet_id;
            bool broadcast;
            uint8_t seq_cnt;
        } START; ///< ARES_FRAME_START

        struct {
            uint32_t freq_hz;
            uint16_t preamble_len;
            uint8_t bandwidth;
            uint8_t data_rate;
            uint8_t coding_rate;
            int8_t tx_pow_dbm;
        } LORA_CONFIG; ///< ARES_FRAME_LORA_CONFIG

        struct {
            struct {
                uint8_t ready : 1;
                uint8_t broadcast : 1;
                uint8_t padding : 6;
            } flags;
            uint8_t tx_count;
            uint16_t id;
        } HEARTBEAT; ///< ARES_FRAME_HEARTBEAT

        uint8_t LED; ///< ARES_FRAME_LED

        uint16_t CLAIM; ///< ARES_FRAME_CLAIM

        struct {
            bool broadcast;
            uint8_t tx_cnt;
            uint8_t part;
            uint8_t num_parts;
            uint16_t id;
            size_t msg_len;
            const char *msg; // This must remain valid for
                             // the lifetime of the frame.
        } LOG;               ///< ARES_FRAME_LOG

        int ACK; ///< ARES_FRAME_ACK

        enum ares_frame_error FRAMING_ERROR; ///< ARES_FRAME_FRAMING_ERROR
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

#endif // ARES_SERIAL_FRAME_H