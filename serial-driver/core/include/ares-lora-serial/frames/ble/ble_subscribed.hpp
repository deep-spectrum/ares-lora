/**
 * @file ble_subscribed.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_SUBSCRIBED_HPP
#define ARES_BLE_SUBSCRIBED_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct BleSubscribed
 *
 * Payload data for AresFrame::BLE_SUBSCRIBED frames.
 */
struct BleSubscribed : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = BLE_SUBSCRIBED;

    /**
     * Neighbor states characteristic subscription status.
     */
    bool neighbor_states = false;

    /**
     * Bandwidth characteristic subscription status.
     */
    bool bandwidth = false;

    /**
     * Center frequency characteristic subscription status.
     */
    bool center_frequency = false;

    /**
     * Reference level characteristic subscription status.
     */
    bool ref_lvl = false;

    /**
     * Duration characteristic subscription status.
     */
    bool duration = false;

    /**
     * Description characteristic subscription status.
     */
    bool description = false;

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

#endif // ARES_BLE_SUBSCRIBED_HPP
