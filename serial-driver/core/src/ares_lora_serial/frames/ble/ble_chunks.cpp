/**
 * @file ble_chunks.cpp
 *
 * @brief
 *
 * @date 7/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/ble/ble_chunks.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
size_t BleChunk::payload_size() { return _payload_size; }

void BleChunk::serialize(std::vector<uint8_t> &buffer) {
    ares::serialize(buffer, num_chunks);
}
} // namespace AresFrame
