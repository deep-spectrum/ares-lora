/**
 * @file packet_rx.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/debug/packet_rx.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
void PktRx::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        // todo
    }

    ares::deserialize(buffer, seq_cnt, packet_id, src_id);
}
} // namespace AresFrame
