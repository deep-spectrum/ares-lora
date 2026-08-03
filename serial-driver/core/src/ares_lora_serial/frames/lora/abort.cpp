/**
 * @file abort.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/abort.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
size_t Abort::payload_size() { return _payload_size; }

void Abort::serialize(std::vector<uint8_t> &buffer) {
    uint8_t flags = 0;
    ares::set_flags(flags, broadcast);
    ares::serialize(buffer, flags, id);
}

void Abort::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        throw AresFrameError("Invalid payload size received");
    }

    uint8_t flags;
    ares::deserialize(buffer, flags, id);
}
} // namespace AresFrame
