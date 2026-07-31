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

#include <ares-lora-serial/frames/frame.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct LoraAck : Internal::FramePayloadBase {
    /**
     * On transmission, the node id the ack message should be directed to.
     * On reception, the node id the ack message came from.
     */
    uint16_t id = 0;

    /**
     * The message being acknowledged.
     */
    AresFrameType message_type = UNKNOWN;

    size_t payload_size() override;
    void serialize(std::vector<uint8_t> &buffer) override;
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 4;
};
} // namespace AresFrame

#endif // ARES_LORA_ACK_HPP
