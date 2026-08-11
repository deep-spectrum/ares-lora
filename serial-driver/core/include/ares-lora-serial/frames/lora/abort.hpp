/**
 * @file abort.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_ABORT_HPP
#define ARES_ABORT_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct Abort
 * Payload data for AresFrame::ABORT frames.
 */
struct Abort : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = ABORT;

    /**
     * Constructor.
     */
    Abort() = default;

    /**
     * Constructor.
     * @param broadcast See Abort::broadcast.
     * @param id See Abort::id.
     */
    explicit Abort(bool broadcast, uint16_t id)
        : broadcast(broadcast), id(id) {}

    /**
     * On transmission, indicate if the message should be broadcasted. On
     * reception, indicates if the message was broadcasted.
     */
    bool broadcast = false;

    /**
     * On transmission, the node id to send the abort message to if being
     * directed. On reception, the node id the message came from.
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

  private:
    static constexpr size_t _payload_size = 3;
};
} // namespace AresFrame

#endif // ARES_ABORT_HPP
