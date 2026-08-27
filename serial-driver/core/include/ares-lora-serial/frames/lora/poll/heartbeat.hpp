/**
 * @file heartbeat.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_HEARTBEAT_HPP
#define ARES_HEARTBEAT_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/lora_base.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {

/**
 * @struct Heartbeat
 *
 * Data for AresFrame::HEARTBEAT frames.
 */
struct Heartbeat : Internal::FramePayloadBase, Internal::LoraBase {
    static constexpr AresFrameType frame_type = HEARTBEAT;

    /**
     * Constructor.
     */
    Heartbeat() = default;

    /**
     * Constructor.
     * @param ready See Heartbeat::ready.
     * @param id See Heartbeat::id.
     */
    explicit Heartbeat(bool ready, uint16_t id) : ready(ready), id(id) {}

    /**
     * System ready for data collection.
     */
    bool ready = false;

    /**
     * The destination or source of the heartbeat.
     */
    uint16_t id = 0;

    /**
     * Payload size.
     * @return The payload size.
     */
    std::size_t payload_size() override;

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
     * Equivalence operator.
     * @param[in] rhs The other object to compare against.
     * @return `true` if all applicable fields are equal, `false` otherwise.
     */
    bool operator==(const Heartbeat &rhs) const;

  private:
    static constexpr std::size_t _payload_size = 3;
};
} // namespace AresFrame

#endif // ARES_HEARTBEAT_HPP
