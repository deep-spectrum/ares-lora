/**
 * @file ble_config_request.hpp
 *
 * @brief
 *
 * @date 11/9/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_CONFIG_REQUEST_HPP
#define ARES_BLE_CONFIG_REQUEST_HPP

#include <ares-lora-serial/frames/ble/ble_types.hpp>
#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct BleNodeConfiguration
 * Payload data for AresFrame::BLE_CONFIG_REQ frames.
 */
struct BleConfigRequest : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = BLE_CONFIG_REQ;

    /**
     * Configuration type.
     */
    BleConfigurationTypes type = BANDWIDTH;

    /**
     * Decode the payload from a serial buffer.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     */
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = sizeof(type);
};
} // namespace AresFrame

#endif // ARES_BLE_CONFIG_REQUEST_HPP
