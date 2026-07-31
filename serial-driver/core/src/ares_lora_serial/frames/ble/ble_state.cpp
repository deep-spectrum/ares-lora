/**
 * @file ble_state.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/ble/ble_state.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
size_t BleState::payload_size() { return _payload_size; }

void BleState::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, state);
}

void BleState::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        // todo
    }

    ares::deserialize(buffer, state);
}
} // namespace AresFrame
