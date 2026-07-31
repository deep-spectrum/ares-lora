/**
 * @file setting.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/mcu/setting.hpp>
#include <ares/serialization.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

namespace AresFrame {
std::size_t Setting::payload_size() {
    return set ? set_payload_size : get_payload_size;
}

void Setting::preprocess() {
    // nop
}

void Setting::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, setting_id);

    if (set) {
        ares::serialize(buffer, value);
    }
}

void Setting::deserialize(const uint8_t *payload, std::size_t len) {
    if (len != set_payload_size) {
        // todo: throw error
    }

    ares::deserialize(payload, setting_id, value);
}

bool Setting::new_frame() { return false; }
} // namespace AresFrame
