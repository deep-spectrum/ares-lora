/**
 * @file ble_disconnect.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_DISCONNECTED_HPP
#define ARES_BLE_DISCONNECTED_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct BleDisconnect
 *
 * Payload data for AresFrame::BLE_DISCONNECT frames.
 */
struct BleDisconnect : Internal::FramePayloadBase {
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
    static constexpr size_t _payload_size = 0;
};
} // namespace AresFrame

#endif // ARES_BLE_DISCONNECTED_HPP
