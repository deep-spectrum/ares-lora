/**
 * @file ble_config_request.cpp
 *
 * @brief
 *
 * @date 11/9/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/ble/ble_config_request.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
void BleConfigRequest::deserialize(const uint8_t *buffer, std::size_t len) {
    if (len != _payload_size) {
        throw AresFrameError("Invalid payload size received");
    }

    ares::deserialize(buffer, type);
}
} // namespace AresFrame
