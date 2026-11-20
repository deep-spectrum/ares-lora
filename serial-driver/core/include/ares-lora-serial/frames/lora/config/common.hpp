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

#include <ares/datetime/datetime.hpp>
#include <cstdint>
#include <variant>

namespace AresFrame {

/**
 * @typedef NodeConfigData
 *
 * A variant representing all of the configuration data types.
 */
using NodeConfigData =
    std::variant<std::monostate, ares::DateTime, uint32_t, double>;

namespace Internal {
/**
 * Max size of the config part of the payload.
 */
constexpr std::size_t NodeConfigDataSizeof = 8;

/**
 * Encode the save folder configuration.
 * @param config The save folder configuration to encode.
 * @param config_item The container to encode the save folder name into.
 */
void serialize_save_folder(const ares::DateTime &config, uint64_t &config_item);

/**
 * Decode the save folder configuration.
 * @param config The save folder configuration container.
 * @param config_item The container to decode the save folder configuration
 * from.
 */
void deserialize_save_folder(ares::DateTime &config, uint64_t config_item);
} // namespace Internal
} // namespace AresFrame

#endif // ARES_COMMON_HPP
