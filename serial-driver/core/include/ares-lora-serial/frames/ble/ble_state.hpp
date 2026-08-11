/**
 * @file ble_state.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_STATE_HPP
#define ARES_BLE_STATE_HPP

#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
/**
 * @struct BleState
 *
 * Payload data for AresFrame::BLE_STATE frames.
 */
struct BleState : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = BLE_STATE;

    /**
     * @enum State
     *
     * BLE States.
     */
    enum State : uint8_t {
        OFF = 0,     ///< BLE off.
        ON = 1,      ///< BLE On.
        REQUEST = 2, ///< Request BLE state.
    };

    /**
     * Default constructor.
     */
    BleState() = default;

    /**
     * Constructor.
     * @param[in] value The state value.
     */
    explicit BleState(uint8_t value) { state = static_cast<State>(value); }

    /**
     * Constructor.
     * @param value The state value.
     */
    explicit BleState(State value) : state(value) {}

    /**
     * The BLE state frame data.
     */
    State state = REQUEST;

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
    static constexpr size_t _payload_size = 1;
};
} // namespace AresFrame

#endif // ARES_BLE_STATE_HPP
