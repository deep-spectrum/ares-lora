/**
 * @file packet_tx.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/debug/packet_tx.hpp>
#include <ares/serialization.hpp>

void AresFrame::PktTx::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        // todo
    }

    ares::deserialize(buffer, count);
}
