/**
 * @file poll.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_POLL_HPP
#define ARES_POLL_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/lora_base.hpp>
#include <ares-lora-serial/frames/lora/poll/heartbeat.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct Poll
 *
 * Data for AresFrame::POLL frames.
 */
struct Poll : Internal::FramePayloadBase, Internal::LoraBase {
    static constexpr AresFrameType frame_type = POLL;
    using response_type = Heartbeat;

    /**
     * Constructor.
     */
    Poll() = default;

    /**
     * Constructor.
     * @param id See Poll:id.
     */
    explicit Poll(uint16_t id) : id(id) {}

    /**
     * When transmitting, the ID of the node to poll for a heartbeat. When
     * receiving, the source of the poll message.
     */
    uint16_t id = 0;

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

    /**
     * Retrieve the expected response message.
     * @return The expected response message.
     */
    [[nodiscard]] response_type expected_response() const;

  private:
    static constexpr size_t _payload_size = 2;
};
} // namespace AresFrame

#endif // ARES_POLL_HPP
