/**
 * @file ble_types.hpp
 *
 * @brief
 *
 * @date 11/6/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_TYPES_HPP
#define ARES_BLE_TYPES_HPP

#include <cstdint>

namespace AresFrame {
/**
 * @enum BlePhy
 * BLE Physical layer options.
 */
enum BlePhy : uint8_t {
    PHY_2M = 0,       ///< 2Mbit/sec
    PHY_1M = 1,       ///< 1Mbit/sec
    PHY_S2_CODED = 2, ///< Coded S=2
    PHY_S8_CODED = 3, ///< Coded S=8
};
} // namespace AresFrame

#endif // ARES_BLE_TYPES_HPP
