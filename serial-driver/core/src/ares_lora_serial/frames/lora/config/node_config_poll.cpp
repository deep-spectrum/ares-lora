/**
 * @file node_config_poll.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/lora/config/node_config_poll.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
size_t NodeConfigPoll::payload_size() { return _payload_size; }

void NodeConfigPoll::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, id, type);
}

void NodeConfigPoll::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        // todo
    }

    ares::deserialize(buffer, id, type);
}
} // namespace AresFrame
