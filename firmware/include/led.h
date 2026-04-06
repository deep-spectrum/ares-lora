/**
 * @file led.h
 *
 * @brief
 *
 * @date 4/6/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LED_H
#define ARES_LED_H

#include <stdint.h>

enum led_state { OFF, ON, BLINK, LED_INVALID };

int update_led_state(uint8_t new_state);

#endif // ARES_LED_H
