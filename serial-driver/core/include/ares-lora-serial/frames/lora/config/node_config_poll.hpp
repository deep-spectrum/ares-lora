/**
 * @file node_config_poll.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_NODE_CONFIG_POLL_HPP
#define ARES_NODE_CONFIG_POLL_HPP

#include <ares-lora-serial/frames/lora/config/common.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct NodeConfigPoll : Internal::FramePayloadBase {
    /**
     * On transmission, the node id to poll for a configuration. On
     * reception, the node id the poll request is coming from.
     */
    uint16_t id = 0;

    /**
     * The configuration type being polled for.
     */
    NodeConfigType type = INVALID;

    size_t payload_size() override;
    void serialize(std::vector<uint8_t> &buffer) override;
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 3;
};
} // namespace AresFrame

#endif // ARES_NODE_CONFIG_POLL_HPP
