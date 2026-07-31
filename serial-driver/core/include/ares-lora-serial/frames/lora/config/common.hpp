/**
 * @file common.hpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_COMMON_HPP
#define ARES_COMMON_HPP

#include <cstdint>
#include <variant>
#include <vector>

namespace AresFrame {
/**
 * @enum NodeConfigType
 * The type of configurations that can be sent over LoRa.
 */
enum NodeConfigType : uint8_t {
    /**
     * The save folder name.
     */
    SAVE_FOLDER,

    /**
     * The bandwidth.
     */
    BANDWIDTH,

    /**
     * The center frequency.
     */
    CENTER_FREQ,

    /**
     * The duration.
     */
    DURATION,

    /**
     * The reference level.
     */
    REF_LEVEL,

    INVALID,
};

/**
 * @struct NodeConfigSaveFolder
 * The save folder configurations.
 */
struct NodeConfigSaveFolder {
    uint16_t year = 0;  ///< The calendar year.
    uint8_t month = 0;  ///< The calendar month.
    uint8_t day = 0;    ///< The calendar day.
    uint8_t hour = 0;   ///< The hour of the day.
    uint8_t minute = 0; ///< The minute in the hour.
    uint8_t second = 0; ///< The second in the minute.
};

using NodeConfigData =
    std::variant<std::monostate, NodeConfigSaveFolder, uint32_t, double>;

namespace Internal {
constexpr std::size_t NodeConfigDataSizeof = 8;

void serialize_save_folder(const NodeConfigSaveFolder &config,
                           uint64_t &config_item);
void deserialize_save_folder(NodeConfigSaveFolder &config,
                             uint64_t config_item);
} // namespace Internal
} // namespace AresFrame

#endif // ARES_COMMON_HPP
