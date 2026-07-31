/**
 * @file ble_connected.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/ble/ble_connected.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
void BleConnect::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        // todo
    }

    uint8_t flags;
    ares::deserialize(buffer, flags, chunk_size);
    connected = ares::check_bit(flags, 0);
}
} // namespace AresFrame
