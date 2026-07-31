/**
 * @file ble_subscribed.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/ble/ble_subscribed.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
void BleSubscribed::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        // todo
    }

    uint8_t flags;
    ares::deserialize(buffer, flags);
    ares::get_flags(flags, chunk, image);
}
} // namespace AresFrame
