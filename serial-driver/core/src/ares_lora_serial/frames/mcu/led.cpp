/**
 * @file led.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/mcu/led.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
std::size_t Led::payload_size() { return _payload_size; }

void Led::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, led, state);
}

void Led::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        throw AresFrameError("Invalid payload size received");
    }

    ares::deserialize(buffer, led, state);
}
} // namespace AresFrame
