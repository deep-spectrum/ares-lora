/**
 * @file version.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_VERSION_HPP
#define ARES_VERSION_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct Version
 *
 * Data for AresFrame::VERSION frames.
 */
struct Version : Internal::FramePayloadBase {
    /**
     * The application version.
     */
    uint32_t app = 0;

    /**
     * The Nordic Connect SDK version.
     */
    uint32_t ncs = 0;

    /**
     * The Zephyr RTOS kernel version.
     */
    uint32_t kernel = 0;

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
    static constexpr size_t _payload_size = 12;
};
} // namespace AresFrame

#endif // ARES_VERSION_HPP
