/**
 * @file ble_disconnect.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/ble/ble_disconnect.hpp>
#include <ares/serialization.hpp>
#include <ares/util.h>

namespace AresFrame {
size_t BleDisconnect::payload_size() { return _payload_size; }

void BleDisconnect::serialize(std::vector<uint8_t> &buffer) {
    ARG_UNUSED(buffer);
}
} // namespace AresFrame
