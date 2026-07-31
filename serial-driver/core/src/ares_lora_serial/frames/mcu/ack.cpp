/**
 * @file ack.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/mcu/ack.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
void Ack::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        // todo
    }

    ares::deserialize(buffer, code);
}
} // namespace AresFrame
