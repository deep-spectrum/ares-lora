/**
 * @file ble_config.hpp
 *
 * @brief
 *
 * @date 11/6/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_CONFIG_HPP
#define ARES_BLE_CONFIG_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>
#include <string>
#include <variant>

namespace AresFrame {
/**
 * @struct BleNodeConfiguration
 * Payload data for AresFrame::BLE_CONFIG frames.
 */
struct BleNodeConfiguration : Internal::FramePayloadBase {
    static constexpr size_t frame_type = BLE_CONFIG;

    /**
     * Configuration type.
     */
    NodeConfigType type = BANDWIDTH;

    /**
     * Configuration data.
     */
    std::variant<std::monostate, double, uint32_t, std::string> config = {};

    /**
     * Decode the payload from a serial buffer.
     * @param buffer Pointer to buffer that contains encoded payload
     * @param len The size of the payload.
     */
    void deserialize(const uint8_t *buffer, std::size_t len) override;

    /**
     * Encode into a buffer.
     * @param buffer The buffer to place data into.
     */
    void serialize(std::vector<uint8_t> &buffer) override;

    /**
     * Payload size.
     * @return The payload size.
     */
    size_t payload_size() override;

    /**
     * Get the configuration as a double.
     * @return configuration as double.
     */
    [[nodiscard]] double as_double() const;

    /**
     * Get the configuration as an unsigned 32-bit integer.
     * @return configuration as uint32_t
     */
    [[nodiscard]] uint32_t as_uint32() const;

    /**
     * Get the configuration as a string.
     * @return configuration as a string.
     */
    [[nodiscard]] std::string as_str() const;

  private:
    static constexpr size_t _overhead = 2;
};
} // namespace AresFrame

#endif // ARES_BLE_CONFIG_HPP
