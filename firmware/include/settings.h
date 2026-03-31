/**
 * @file settings.h
 *
 * @brief
 *
 * @date 3/27/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_SETTINGS_H
#define ARES_SETTINGS_H

#include <zephyr/kernel.h>

#define FOREACH_ARES_SETTING(FUNC)                                             \
    FUNC(ID, 0, 1, 0xFFFF)                                                     \
    FUNC(WAIT_USB_HOST, 0, 0, 1)                                               \
    FUNC(PANID, 0, 0, 0xFFFF)                                                  \
    FUNC(REPCNT, 10, 1, 0xFFFFFFFF)

#define GENERATE_ENUM(setting_, ...) UTIL_CAT(ARES_SETTING_, setting_),

enum ares_setting { FOREACH_ARES_SETTING(GENERATE_ENUM) ARES_SETTING_RESERVED };

#undef GENERATE_ENUM

int update_setting(enum ares_setting setting, uint32_t value);
int retrieve_setting(enum ares_setting setting, uint32_t *value);
void reset_settings(void);

#endif // ARES_SETTINGS_H