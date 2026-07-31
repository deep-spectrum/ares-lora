/**
 * @file log_ack.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/lora/log/log_ack.hpp>
#include <ares/serialization.hpp>

void AresFrame::LogAck::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        // todo
    }

    ares::deserialize(buffer, part, num_parts, id, log_id);
}
