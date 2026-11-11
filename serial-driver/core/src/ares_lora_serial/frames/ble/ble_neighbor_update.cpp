/**
 * @file ble_neighbor_update.cpp
 *
 * @brief
 *
 * @date 11/9/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/frames/ble/ble_neighbor_update.hpp>
#include <ares/serialization.hpp>

namespace AresFrame {
void BleNeighborUpdate::serialize(std::vector<uint8_t> &buffer) {
    uint16_t num_neighbors = neighbor_data.size();

    ares::serialize(buffer, num_neighbors);
    for (const auto &i : neighbor_data) {
        ares::serialize(buffer, i.id, i.ready);
    }

    assert(buffer.size() == payload_size());
}

size_t BleNeighborUpdate::payload_size() {
    return _overhead + (neighbor_data.size() * _bytes_per_neighbor);
}
} // namespace AresFrame
