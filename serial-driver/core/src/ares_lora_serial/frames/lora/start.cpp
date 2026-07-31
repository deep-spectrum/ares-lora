/**
 * @file start.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/lora/start.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
std::size_t Start::payload_size() { return _payload_size; }

void Start::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, sec, usec, id, broadcast, seq_cnt, packet_id);
}

void Start::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len > _payload_size) {
        // todo
    }

    ares::deserialize(buffer, sec, usec, id, broadcast, seq_cnt, packet_id);
}
} // namespace AresFrame
