/**
 * @file heartbeat.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/poll/heartbeat.hpp>
#include <ares/serialization.hpp>
#include <cassert>

namespace AresFrame {
std::size_t Heartbeat::payload_size() { return _payload_size; }

void Heartbeat::serialize(std::vector<uint8_t> &buffer) {
    uint8_t flags = 0;
    ares::set_flags(flags, ready);

    ares::serialize(buffer, flags, id);
    assert(buffer.size() == _payload_size);
}

void Heartbeat::deserialize(const uint8_t *buffer, std::size_t len) {

    if (len != _payload_size) {
        throw AresFrameError("Invalid payload size received");
    }

    uint8_t flags;

    ares::deserialize(buffer, flags, id);
    ares::get_flags(flags, ready);
}

bool Heartbeat::operator==(const Heartbeat &rhs) const { return id == rhs.id; }
} // namespace AresFrame
