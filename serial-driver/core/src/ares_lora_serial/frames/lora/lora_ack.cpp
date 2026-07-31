/**
 * @file lora_ack.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/lora/lora_ack.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
size_t LoraAck::payload_size() { return _payload_size; }

void LoraAck::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, id, message_type);
}

void LoraAck::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        // todo
    }

    ares::deserialize(buffer, id, message_type);
}
} // namespace AresFrame
