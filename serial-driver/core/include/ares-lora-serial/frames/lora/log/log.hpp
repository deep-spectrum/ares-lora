/**
 * @file log.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LOG_HPP
#define ARES_LOG_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {

/**
 * @struct Log
 *
 * Data for AresFrame::LOG frames.
 */
struct Log : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = LOG;

    /**
     * Constructor.
     * @param broadcast Flag indicating if the message should be
     * broadcasted.
     * @param tx_cnt The number of times to send the message over LoRa.
     * @param id The destination or source ID.
     * @param log_id The ID of the log message.
     * @param msg The log message.
     */
    explicit Log(bool broadcast, uint8_t tx_cnt, uint16_t id, uint16_t log_id,
                 std::string msg)
        : broadcast(broadcast), tx_cnt(tx_cnt), id(id), log_id(log_id),
          msg(std::move(msg)) {}

    /**
     * Default constructor.
     */
    Log() = default;

    /**
     * Flag indicating if the message was/should be broadcasted.
     */
    bool broadcast = false;

    /**
     * The number of times to transmit the message.
     */
    uint8_t tx_cnt = 1;

    /**
     * The chunk ID of the message. 1 indexed and automatically managed on
     * serialization.
     */
    uint8_t part = 1;

    /**
     * The number of chunks in the log message. Automatically managed on
     * serialization.
     */
    uint8_t num_parts = 1;

    /**
     * On transmission, the ID of the node to send the message to if not
     * broadcasting. On reception, the ID of the node the message was sent
     * from.
     */
    uint16_t id = 0;

    /**
     * The ID of the log message.
     */
    uint16_t log_id = 0;

    /**
     * The entire log message on transmission. The log message chunk on
     * reception.
     */
    std::string msg;

    /**
     * Payload size.
     * @return The payload size.
     */
    size_t payload_size() override;

    /**
     * Preprocess The payload.
     */
    void preprocess() override;

    /**
     * Encode into a buffer.
     * @param buffer The buffer to place data into.
     */
    void serialize(std::vector<uint8_t> &buffer) override;

    /**
     * Decode the payload from a serial buffer.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     */
    void deserialize(const uint8_t *buffer, std::size_t len) override;

    /**
     * Check if there is a new frame available after preprocessing.
     * @return @p true if there is a new frame, @p false otherwise.
     */
    bool new_frame() override;

    /**
     * Get the number of frames needed.
     * @return Number of frames.
     */
    [[nodiscard]] size_t num_frames() const override;

  private:
    std::vector<std::string> _msg_split;
    size_t _idx = 0;
    uint8_t _part = 0;
    uint8_t _num_parts = 1;
    bool _preprocessed = false;

    static constexpr size_t _overhead = 8;
    static constexpr size_t _max_payload_size = 32;
};
} // namespace AresFrame

#endif // ARES_LOG_HPP
