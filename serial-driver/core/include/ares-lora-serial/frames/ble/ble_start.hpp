/**
 * @file ble_start.hpp
 *
 * @brief
 *
 * @date 11/9/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_START_HPP
#define ARES_BLE_START_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct BleStart
 * Payload data for AresFrame::BLE_START frames.
 */
struct BleStart : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = BLE_START;

    /**
     * Amount of time to wait in seconds before starting run.
     */
    uint32_t delay = 0;

    /**
     * Decode the payload from a serial buffer.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     */
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = sizeof(delay);
};
} // namespace AresFrame

#endif // ARES_BLE_START_HPP
