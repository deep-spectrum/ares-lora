/**
 * @file led.h
 *
 * @brief LED API.
 *
 * @date 4/6/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LED_H
#define ARES_LED_H

#include <stdint.h>

/**
 * @enum led_state
 * Different states the LED can be in.
 */
enum led_state {
    LED_STATE_OFF,     ///< LED off.
    LED_STATE_ON,      ///< LED on.
    LED_STATE_BLINK,   ///< LED blinking.
    LED_STATE_FADE,    ///< LED Fading on and off.
    LED_STATE_INVALID, ///< Invalid/end of enumeration.
};

/**
 * @enum led
 * LEDs that can be controlled.
 */
enum led {
    LED_0,       ///< LED 0
    LED_INVALID, ///< Last LED enumerator
};

/**
 * Update or retrieve the LED state.
 *
 * @param[in] led The LED to update the state for.
 * @param[in] new_state The new state of the LED. If invalid, retrieves the
 * current state.
 *
 * @return The current state of the LED if `new_state` is invalid.
 * @return 0 if new state was set successfully.
 * @return -EAGAIN if state is unchanged from previous setting.
 * @return negative error code otherwise.
 */
int update_led_state(enum led led, uint8_t new_state);

#endif // ARES_LED_H
