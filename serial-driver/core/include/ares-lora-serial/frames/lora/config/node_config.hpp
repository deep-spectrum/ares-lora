/**
 * @file node_config.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_NODE_CONFIG_HPP
#define ARES_NODE_CONFIG_HPP

#include <ares-lora-serial/frames/lora/config/common.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct NodeConfig : Internal::FramePayloadBase {
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

    size_t payload_size() override;
    void serialize(std::vector<uint8_t> &buffer) override;
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr size_t _payload_size = 3 + Internal::NodeConfigDataSizeof;
};
} // namespace AresFrame

#endif // ARES_NODE_CONFIG_HPP
