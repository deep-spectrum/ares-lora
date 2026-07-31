/**
 * @file led.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LED_HPP
#define ARES_LED_HPP

#include <ares-lora-serial/frames/payload_base.hpp>

namespace AresFrame {
struct Led : Internal::FramePayloadBase {
    /**
     * @enum LedState
     *
     * LED states.
     */
    enum LedState : uint8_t {
        OFF = 0,   ///< LED off.
        ON = 1,    ///< LED on.
        BLINK = 2, ///< LED blinking at 1 Hz.
        FADE = 3,  ///< LED fading.
        FETCH = 4, ///< Retrieve LED state from firmware.
    };

    /**
     * The LED number/ID.
     */
    uint8_t led = 0;

    /**
     * The LED state frame data.
     */
    LedState state = FETCH;

    std::size_t payload_size() override;
    void serialize(std::vector<uint8_t> &buffer) override;
    void deserialize(const uint8_t *buffer, std::size_t len) override;

  private:
    static constexpr std::size_t _payload_size = 2;
};
} // namespace AresFrame

#endif // ARES_LED_HPP
