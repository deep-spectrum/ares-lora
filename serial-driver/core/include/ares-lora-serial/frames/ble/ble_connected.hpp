/**
 * @file ble_connected.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_CONNECTED_HPP
#define ARES_BLE_CONNECTED_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct BleConnect : Internal::FramePayloadBase {

    /**
     * Flag indicating if the BLE is connected or not.
     */
    bool connected = false;

    /**
     * The maximum transfer size.
     */
    uint16_t chunk_size = 0;

    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 3;
};
} // namespace AresFrame

#endif // ARES_BLE_CONNECTED_HPP
