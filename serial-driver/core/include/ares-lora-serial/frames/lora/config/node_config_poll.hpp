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

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/lora/config/common.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct NodeConfigPoll
 * Payload for AresFrame::NODE_CONFIG_POLL frames
 */
struct NodeConfigPoll : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = NODE_CONFIG_POLL;

    /**
     * Constructor.
     */
    NodeConfigPoll() = default;

    /**
     * Constructor.
     * @param id See NodeConfigPoll::id
     * @param type see NodeConfigPoll::type
     */
    explicit NodeConfigPoll(uint16_t id, NodeConfigType type)
        : id(id), type(type) {}

    /**
     * On transmission, the node id to poll for a configuration. On
     * reception, the node id the poll request is coming from.
     */
    uint16_t id = 0;

    /**
     * The configuration type being polled for.
     */
    NodeConfigType type = INVALID;

    /**
     * Payload size.
     * @return The payload size.
     */
    size_t payload_size() override;

    /**
     * Encode into a buffer.
     * @param buffer The buffer to place data into.
     */
    void serialize(std::vector<uint8_t> &buffer) override;

    /**
     * Decode the payload from a serial buffer.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     */
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 3;
};
} // namespace AresFrame

#endif // ARES_NODE_CONFIG_POLL_HPP
