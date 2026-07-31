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

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct BleSubscribed : Internal::FramePayloadBase {
    /**
     * Flag indicating if the chunks attribute has been subscribed to.
     */
    bool chunk = false;

    /**
     * Flag indicating if the image attribute has been subscribed to.
     */
    bool image = false;

    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 1;
};
} // namespace AresFrame

#endif // ARES_BLE_SUBSCRIBED_HPP
