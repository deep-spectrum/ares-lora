/**
 * @file reboot.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_REBOOT_HPP
#define ARES_REBOOT_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct Reboot
 *
 * Payload data for AresFrame::REBOOT frames.
 */
struct Reboot : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = REBOOT;

    /**
     * Constructor.
     */
    Reboot() = default;

    /**
     * Constructor.
     * @param delay See Reboot::delay.
     */
    explicit Reboot(uint8_t delay) : delay(delay) {}

    /**
     * The delay in seconds before the reboot occurs.
     */
    uint8_t delay = 5;

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

  private:
    static constexpr size_t _payload_size = 1;
};
} // namespace AresFrame

#endif // ARES_REBOOT_HPP
