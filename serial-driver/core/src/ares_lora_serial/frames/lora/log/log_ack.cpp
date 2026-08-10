/**
 * @file log_ack.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/log/log_ack.hpp>
#include <ares/serialization.hpp>
#include <cassert>

void AresFrame::LogAck::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, part, num_parts, id, log_id);
    assert(buffer.size() == _payload_size);
}

void AresFrame::LogAck::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        throw AresFrameError("Invalid payload size received");
    }

    ares::deserialize(buffer, part, num_parts, id, log_id);
}
