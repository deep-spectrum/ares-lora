/**
 * @file frame.h
 *
 * @brief Ares frame library.
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

/**
 * Header for Ares frames.
 */
#define ARES_FRAME_HEADER '^'

/**
 * Footer for Ares frames.
 */
#define ARES_FRAME_FOOTER '@'

/**
 * Number of bytes needed for the header field.
 */
#define ARES_FRAME_HEADER_OVERHEAD UINT32_C(1)

/**
 * Number of bytes needed for the type field.
 */
#define ARES_FRAME_TYPE_OVERHEAD UINT32_C(1)

/**
 * Number of bytes needed for the length field.
 */
#define ARES_FRAME_LEN_OVERHEAD UINT32_C(2)

/**
 * Number of bytes needed for the footer field.
 */
#define ARES_FRAME_FOOTER_OVERHEAD UINT32_C(1)

/**
 * Number of bytes needed for an Ares frame.
 */
#define ARES_FRAME_OVERHEAD                                                    \
    (uint64_t)(ARES_FRAME_HEADER_OVERHEAD + ARES_FRAME_TYPE_OVERHEAD +         \
               ARES_FRAME_LEN_OVERHEAD + ARES_FRAME_FOOTER_OVERHEAD)

/**
 * @enum ares_frame_error
 * @brief Frame related errors.
 */
enum ares_frame_error {
    ARES_FRAME_ERROR_BAD_FRAME = 0, ///< Frame could not be deserialized.
    ARES_FRAME_ERROR_BAD_TYPE = 1,  ///< Frame type not valid for reception.
    ARES_FRAME_ERROR_NOT_IMPLEMENTED = 2, ///< Frame handler not implemented.
};

/**
 * @enum ares_frame_type
 * @brief Frame types.
 */
enum ares_frame_type {
    ARES_FRAME_SETTING,       ///< SETTING frame.
    ARES_FRAME_START,         ///< Start time frame.
    ARES_FRAME_LORA_CONFIG,   ///< LoRa configuration frame.
    ARES_FRAME_LED,           ///< Control LED state.
    ARES_FRAME_HEARTBEAT,     ///< LoRa Heart Beat frame.
    ARES_FRAME_CLAIM,         ///< LoRa host claim frame.
    ARES_FRAME_LOG,           ///< Log message.
    ARES_FRAME_LOG_ACK,       ///< Log message ACK from LoRa.
    ARES_FRAME_VERSION,       ///< Version information.
    ARES_FRAME_ACK,           ///< ACK frame.
    ARES_FRAME_FRAMING_ERROR, ///< Framing error frame. TX only.
    ARES_FRAME_DBG,           ///< Debug frames, TX only.

    ARES_FRAME_TYPE_INVALID, ///< Invalid frame.
};

/**
 * @struct ares_frame
 * @brief Structured representation of an ares frame.
 */
struct ares_frame {
    /**
     * Frame type.
     */
    enum ares_frame_type type;

    /**
     * Frame payload.
     */
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
            uint8_t cad_mode;
            uint8_t cad_symbol_num;
            uint8_t cad_detection_peak;
            uint8_t cad_detection_min;
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
            uint16_t log_id;
            size_t msg_len;
            const char *msg; // This must remain valid for
                             // the lifetime of the frame.
        } LOG;               ///< ARES_FRAME_LOG

        struct {
            uint8_t part;
            uint8_t num_parts;
            uint16_t id;
            uint16_t log_id;
        } LOG_ACK; ///< ARES_FRAME_LOG_ACK

        struct {
            uint32_t app;
            uint32_t ncs;
            uint32_t kernel;
        } VERSION; ///< ARES_FRAME_VERSION

        int ACK; ///< ARES_FRAME_ACK

        enum ares_frame_error FRAMING_ERROR; ///< ARES_FRAME_FRAMING_ERROR

        int DBG; ///< ARES_FRAME_DBG
    } payload;
};

/**
 * @struct ares_frame_info
 * @brief Serial buffer metadata that describes the location of a frame in a
 * buffer, the size of the frame, and how many bytes need to be read before
 * deserialization is possible.
 */
struct ares_frame_info {
    /**
     * Start index.
     *
     * @note -1 If no frame header was found.
     */
    int start_index;

    /**
     * Size of the frame.
     *
     * @note -1 if no frame header was found.
     */
    int frame_size;

    /**
     * Number of additional bytes needed for a complete frame.
     *
     * @note -1 if no frame header found.
     */
    int bytes_left;
};

/**
 * @brief Function to serialize an ares frame into a buffer.
 *
 * @param[out] buf Pointer to the destination buffer.
 * @param[in] len The length of the destination buffer.
 * @param[in] frame Pointer to the source frame.
 *
 * @return The number of bytes written to the buffer.
 * @return -EINVAL if invalid parameters.
 * @return -ENOBUFS buffer size is too small.
 */
int ares_serialize_frame(uint8_t *buf, size_t len,
                         const struct ares_frame *frame);

/**
 * @brief Function to deserialize an ares frame from a buffer.
 *
 * @param[out] frame Pointer to the destination frame.
 * @param[in] buf Pointer to the start of the buffered frame.
 * @param[in] len The length of the buffered frame.
 *
 * @return 0 on success.
 * @return -EBADMSG if frame is not valid.
 * @return -EINVAL on invalid parameters.
 */
int ares_deserialize_frame(struct ares_frame *frame, const uint8_t *buf,
                           size_t len);

/**
 * @brief Function to check if there is an ares frame in the buffer.
 *
 * @param[in] buf Pointer to source buffer.
 * @param[in] len The number of valid bytes in the buffer.
 * @param[out] info The metadata for finding the frame in the buffer.
 *
 * @return 1 if frame was found.
 * @return 0 if no frame was found.
 * @return -EINVAL if parameters are invalid.
 */
int ares_serial_frame_present(const uint8_t *buf, size_t len,
                              struct ares_frame_info *info);

/**
 * @brief Check if the frame in the buffer is valid.
 *
 * @param[in] buf Pointer to source buffer.
 * @param[in] len The length of the serialized frame.
 *
 * @return `true` if the frame is valid.
 * @return `false` otherwise.
 */
bool ares_check_if_frame(const uint8_t *buf, size_t len);

#endif // ARES_SERIAL_FRAME_H