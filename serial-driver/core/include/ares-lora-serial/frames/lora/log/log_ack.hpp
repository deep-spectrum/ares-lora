/**
 * @file log_ack.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LOG_ACK_HPP
#define ARES_LOG_ACK_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/lora_base.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {

/**
 * @struct LogAck
 *
 * Data for AresFrame::LOG_ACK frames.
 */
struct LogAck : Internal::FramePayloadBase, Internal::LoraBase {
    static constexpr AresFrameType frame_type = LOG_ACK;

    /**
     * Constructor.
     * @param part See LogAck::part
     * @param num_parts See LogAck::num_parts
     * @param id See LogAck::id
     */
    explicit LogAck(uint8_t part, uint8_t num_parts, uint16_t id)
        : part(part), num_parts(num_parts), id(id) {}

    /**
     * Default constructor.
     */
    LogAck() = default;

    /**
     * The message part that was acknowledged.
     */
    uint8_t part = 0;

    /**
     * The total number of parts in the acked message.
     */
    uint8_t num_parts = 0;

    /**
     * The ID of the node that acknowledged the message.
     */
    uint16_t id = 0;

    /**
     * The ID of the log message that got acked.
     */
    uint16_t log_id = 0;

    /**
     * Equivalence operator.
     * @param other The other object to compare against.
     * @return `true` if all the fields are equal, `false` otherwise.
     */
    bool operator==(const LogAck &other) const {
        return (part == other.part) && (num_parts == other.num_parts) &&
               (id == other.id) && (log_id == other.log_id);
    }

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

  private:
    static constexpr size_t _payload_size = 6;
};
} // namespace AresFrame

#endif // ARES_LOG_ACK_HPP
