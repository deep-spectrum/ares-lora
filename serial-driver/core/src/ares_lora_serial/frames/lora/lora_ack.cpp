/**
 * @file lora_ack.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/lora_ack.hpp>
#include <ares/serialization.hpp>
#include <cassert>

namespace AresFrame {
size_t LoraAck::payload_size() { return _payload_size; }

void LoraAck::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, id, message_type);
    assert(buffer.size() == _payload_size);
}

void LoraAck::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        throw AresFrameError("Invalid payload size received");
    }

    ares::deserialize(buffer, id, message_type);
}
} // namespace AresFrame
