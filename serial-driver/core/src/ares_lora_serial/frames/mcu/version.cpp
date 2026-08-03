/**
 * @file version.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/mcu/version.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
size_t Version::payload_size() { return _payload_size; }

void Version::serialize(std::vector<uint8_t> &buffer) {
    ares::SerializeBuffer<uint32_t, 3> r1;
    ares::serialize(buffer, r1);
}

void Version::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        throw AresFrameError("Invalid payload size received");
    }

    ares::deserialize(buffer, app, ncs, kernel);
}
} // namespace AresFrame
