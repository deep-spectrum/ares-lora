/**
 * @file setting.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/mcu/setting.hpp>
#include <ares/serialization.hpp>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

namespace AresFrame {
std::size_t Setting::payload_size() {
    return set ? set_payload_size : get_payload_size;
}

void Setting::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, setting_id);

    if (set) {
        ares::serialize(buffer, value);
    }

    assert(buffer.size() == payload_size());
}

void Setting::deserialize(const uint8_t *payload, std::size_t len) {
    if (len != set_payload_size) {
        throw AresFrameError("Invalid payload size received");
    }

    ares::deserialize(payload, setting_id, value);
}

} // namespace AresFrame
