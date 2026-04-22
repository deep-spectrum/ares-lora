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
    OFF,         ///< LED off.
    ON,          ///< LED on.
    BLINK,       ///< LED blinking.
    LED_INVALID, ///< Invalid/end of enumeration.
};

/**
 * Update or retrieve the LED state.
 *
 * @param[in] new_state The new state of the LED. If invalid, retrieves the
 * current state.
 *
 * @return The current state of the LED if `new_state` is invalid.
 * @return 0 if new state was set successfully.
 * @return -EAGAIN if state is unchanged from previous setting.
 * @return negative error code otherwise.
 */
int update_led_state(uint8_t new_state);

#endif // ARES_LED_H
