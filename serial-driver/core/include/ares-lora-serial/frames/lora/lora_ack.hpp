/**
 * @file lora_ack.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LORA_ACK_HPP
#define ARES_LORA_ACK_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/lora_base.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct LoraAck
 * Payload data for AresFrame::LORA_ACK frames.
 */
struct LoraAck : Internal::FramePayloadBase, Internal::LoraBase {
    static constexpr AresFrameType frame_type = LORA_ACK;

    /**
     * Constructor.
     */
    LoraAck() = default;

    /**
     * Constructor.
     * @param id See LoraAck::id.
     * @param type Aee LoraAck::type.
     */
    explicit LoraAck(uint16_t id, AresFrameType type)
        : id(id), message_type(type) {}

    /**
     * On transmission, the node id the ack message should be directed to.
     * On reception, the node id the ack message came from.
     */
    uint16_t id = 0;

    /**
     * The message being acknowledged.
     */
    AresFrameType message_type = UNKNOWN;

    /**
     * Payload size.
     * @return The payload size.
     */
    size_t payload_size() override;

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
    static constexpr size_t _payload_size = 4;
};
} // namespace AresFrame

#endif // ARES_LORA_ACK_HPP
