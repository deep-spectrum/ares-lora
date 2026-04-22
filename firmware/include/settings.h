/**
 * @file settings.h
 *
 * @brief Ares-LoRa settings manager.
 *
 * @date 3/27/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_SETTINGS_H
#define ARES_SETTINGS_H

#include <zephyr/kernel.h>

/**
 * The settings for Ares-LoRa, their defaults, and bounds.
 *
 * - Node ID
 * - Wait for USB
 * - Personal Area Network ID
 * - Repetition count
 * .
 *
 * Format: setting name, default value, minimum value, maximum value.
 *
 * @param FUNC The generator macro function.
 */
#define FOREACH_ARES_SETTING(FUNC)                                             \
    FUNC(ID, 0, 1, 0xFFFF)                                                     \
    FUNC(WAIT_USB_HOST, 0, 0, 1)                                               \
    FUNC(PANID, 0, 0, 0xFFFF)                                                  \
    FUNC(REPCNT, 10, 1, 0xFFFFFFFF)

/**
 * Helper for generating the enumerations for Ares-LoRa settings.
 * @param setting_ The setting name.
 */
#define GENERATE_ENUM(setting_, ...) UTIL_CAT(ARES_SETTING_, setting_),

/**
 * @enum ares_setting
 *
 * Represents various settings for Ares-LoRa.
 */
enum ares_setting {
    FOREACH_ARES_SETTING(GENERATE_ENUM)
        ARES_SETTING_RESERVED ///< Reserved settings enumerator indicating the
                              ///< last setting.
};

#undef GENERATE_ENUM

/**
 * Write a new value for a setting.
 *
 * @param[in] setting The setting to update.
 * @param[in] value The new value of the setting.
 *
 * @return 0 on success.
 * @return -EINVAL if setting is invalid.
 * @return -ERANGE if the setting is out of range.
 * @return negative error code otherwise.
 */
int update_setting(enum ares_setting setting, uint32_t value);

/**
 * Read the given setting value.
 *
 * @param[in] setting The setting to read.
 * @param[out] value A pointer to the container for the setting.
 *
 * @return 0 on success.
 * @return -EINVAL if setting is invalid or `value` is NULL.
 */
int retrieve_setting(enum ares_setting setting, uint32_t *value);

/**
 * Resets all the settings to their default values.
 */
void reset_settings(void);

#endif // ARES_SETTINGS_H