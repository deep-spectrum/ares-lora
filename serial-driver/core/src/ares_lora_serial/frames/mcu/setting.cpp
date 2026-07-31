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
#include <cstdint>
#include <vector>
#include <ares/util.h>
#include <cstring>

namespace ares {
    template <typename... Args>
    static void serialize(std::vector<uint8_t> &buffer, Args &&...items) {
        auto process = [&](auto &&item) {
            const auto *val = reinterpret_cast<const uint8_t *>(item);
            buffer.insert(buffer.end(), val, val + sizeof(item));
        };

        (process(std::forward<Args>(items)), ...);
    }

    template <typename... Args>
static void deserialize(const uint8_t *data, Args &&...items) {
        auto process = [&](auto &&item) {
            std::memcpy(&item, data, sizeof(item));
            data += sizeof(item);
        };

        (process(std::forward<Args>(items)), ...);
    }
}

namespace AresFrame {
    std::size_t Setting::payload_size() const {
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

    bool Setting::new_frame() {
        return false;
    }
}
