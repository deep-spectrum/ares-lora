/**
 * @file node_config_response.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_NODE_CONFIG_RESPONSE_HPP
#define ARES_NODE_CONFIG_RESPONSE_HPP

#include <ares-lora-serial/frames/lora/config/common.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct NodeConfigResponse
 * Payload data for AresFrame::NODE_CONFIG_RESPONSE frames.
 */
struct NodeConfigResponse : Internal::FramePayloadBase {
    /**
     * Constructor.
     */
    NodeConfigResponse() = default;

    /**
     *
     * @param id See NodeConfigResponse::id
     * @param type See NodeConfigResponse::type
     * @param config See NodeConfigResponse::config
     */
    explicit NodeConfigResponse(uint16_t id, NodeConfigType type,
                                NodeConfigData config)
        : id(id), type(type), config(config) {}

    /**
     * On transmission, the node id to send the configuration to. On
     * reception, the node id the configurations are coming from.
     */
    uint16_t id = 0;

    /**
     * The configuration type.
     */
    NodeConfigType type = INVALID;

    /**
     * The configuration.
     */
    NodeConfigData config = std::monostate();

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
    static constexpr size_t _payload_size = 3 + Internal::NodeConfigDataSizeof;
};
} // namespace AresFrame

#endif // ARES_NODE_CONFIG_RESPONSE_HPP
