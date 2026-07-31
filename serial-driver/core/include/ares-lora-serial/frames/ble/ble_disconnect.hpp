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
struct BleDisconnect : Internal::FramePayloadBase {

    size_t payload_size() override;
    void serialize(std::vector<uint8_t> &buffer) override;

  private:
    static constexpr size_t _payload_size = 0;
};
} // namespace AresFrame

#endif // ARES_BLE_DISCONNECTED_HPP
