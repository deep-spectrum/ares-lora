/**
 * @file ble_conn_params.hpp
 *
 * @brief
 *
 * @date 11/6/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_CONN_PARAMS_HPP
#define ARES_BLE_CONN_PARAMS_HPP

#include <ares-lora-serial/frames/ble/ble_types.hpp>
#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct BleConnParams
 *
 * Payload data for AresFrame::BLE_CONN_PARAM frames.
 */
struct BleConnParams : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = BLE_CONN_PARAM;

    /**
     * Maximum transfer unit.
     */
    uint16_t mtu = 0;

    /**
     * Connection interval.
     *
     * @note Multiply by 1.25 to get the interval in ms.
     */
    uint16_t interval = 0;

    /**
     * Connection latency.
     */
    uint16_t latency = 0;

    /**
     * Connection supervisor timeout.
     *
     * @note Multiply by 10 to get the timeout in ms.
     */
    uint16_t timeout = 0;

    /**
     * Physical layer.
     */
    BlePhy phy = PHY_1M;

    /**
     * Decode the payload from a serial buffer.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     */
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 9;
};
} // namespace AresFrame

#endif // ARES_BLE_CONN_PARAMS_HPP
