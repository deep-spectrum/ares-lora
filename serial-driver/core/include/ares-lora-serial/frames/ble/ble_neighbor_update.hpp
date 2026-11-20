/**
 * @file ble_neighbor_update.hpp
 *
 * @brief
 *
 * @date 11/9/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_NEIGHBOR_UPDATE_HPP
#define ARES_BLE_NEIGHBOR_UPDATE_HPP

#include <ares-lora-serial/frames/ble/ble_types.hpp>
#include <ares-lora-serial/frames/frame_types.hpp>
#include <ares-lora-serial/frames/payload_base.hpp>
#include <vector>

namespace AresFrame {
/**
 * @struct BleNeighborUpdate
 * Payload data for AresFrame::BLE_NEIGHBOR_UPDATE frames.
 */
struct BleNeighborUpdate : Internal::FramePayloadBase {
    static constexpr AresFrameType frame_type = BLE_NEIGHBOR_UPDATE;

    /**
     * The neighbor data to send over serial.
     */
    std::vector<BleNeighborData> neighbor_data;

    /**
     * Encode into a buffer.
     * @param buffer The buffer to place data into.
     */
    void serialize(std::vector<uint8_t> &buffer) override;

    /**
     * Payload size.
     * @return The payload size.
     */
    size_t payload_size() override;

  private:
    static constexpr size_t _overhead = 2;
    static constexpr size_t _bytes_per_neighbor =
        sizeof(BleNeighborData::id) + sizeof(BleNeighborData::ready);
};
} // namespace AresFrame

#endif // ARES_BLE_NEIGHBOR_UPDATE_HPP
