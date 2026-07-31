/**
 * @file framing_error.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/mcu/framing_error.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
void FramingError::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        // todo
    }

    ares::deserialize(buffer, type);
}
} // namespace AresFrame
