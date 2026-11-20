/**
 * @file ble_att_error.hpp
 *
 * @brief
 *
 * @date 11/6/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_ATT_ERROR_HPP
#define ARES_BLE_ATT_ERROR_HPP

#include <ares-lora-serial/frames/ble/ble_types.hpp>
#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct BleAttErr
 * Payload data for AresFrame::BLE_ATT_ERR frames.
 */
struct BleAttErr : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = BLE_ATT_ERR;

    /**
     * The ATT error code.
     */
    BtAttError code = BT_ATT_ERR_SUCCESS;

    /**
     * Decode the payload from a serial buffer.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     */
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 1;
};
} // namespace AresFrame

#endif // ARES_BLE_ATT_ERROR_HPP
